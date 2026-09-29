// SPDX-License-Identifier: Apache-2.0
// PEAP TLS 1.2 transport. Adapted from ESP-IDF v5.5.2 tls_mbedtls.c
// Copyright (c) 2020-2025 Espressif Systems (Shanghai) CO LTD
// Uses public mbedTLS APIs; TLS records travel over EAP, not a TCP socket.
#include "includes.h"
#include "common.h"
#include "crypto/tls.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/esp_mbedtls_random.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/platform_time.h"
#include "esp_log.h"

#define TLS_BUFFER_LIMIT 65536
struct tls_connection {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt ca;
    struct wpabuf *input, *output;
    size_t input_offset;
    bool configured, failed, exported, verified;
    u8 master[48], random[64];
    mbedtls_tls_prf_types prf;
};

static bool verification_time_ready(void)
{
#if defined(MBEDTLS_HAVE_TIME) && defined(MBEDTLS_HAVE_TIME_DATE)
    // Detect an unset/reset clock. The application must set accurate UTC from a
    // trusted RTC or other trusted source before attempting verified PEAP.
    return mbedtls_time(NULL) >= 1577836800; // 2020-01-01 UTC
#else
    return false;
#endif
}

static int record_write(void *ctx, const unsigned char *buf, size_t len)
{
    struct tls_connection *c = ctx;
    size_t used = c->output ? wpabuf_len(c->output) : 0;
    if (len > TLS_BUFFER_LIMIT - used || wpabuf_resize(&c->output, len) < 0)
        return MBEDTLS_ERR_SSL_ALLOC_FAILED;
    wpabuf_put_data(c->output, buf, len);
    return (int) len;
}

static int record_read(void *ctx, unsigned char *buf, size_t len)
{
    struct tls_connection *c = ctx;
    if (!c->input) return MBEDTLS_ERR_SSL_WANT_READ;
    size_t available = wpabuf_len(c->input) - c->input_offset;
    if (len > available) len = available;
    memcpy(buf, wpabuf_head_u8(c->input) + c->input_offset, len);
    c->input_offset += len;
    if (c->input_offset == wpabuf_len(c->input)) {
        wpabuf_free(c->input); c->input = NULL; c->input_offset = 0;
    }
    return (int) len;
}

static int feed(struct tls_connection *c, const struct wpabuf *input)
{
    if (!input || !wpabuf_len(input)) return 0;
    size_t remaining = c->input ? wpabuf_len(c->input) - c->input_offset : 0;
    if (wpabuf_len(input) > TLS_BUFFER_LIMIT - remaining) return -1;
    struct wpabuf *next = wpabuf_alloc(remaining + wpabuf_len(input));
    if (!next) return -1;
    if (remaining) wpabuf_put_data(next, wpabuf_head_u8(c->input) + c->input_offset, remaining);
    wpabuf_put_buf(next, input);
    wpabuf_free(c->input); c->input = next; c->input_offset = 0;
    return 0;
}

static struct wpabuf *take_output(struct tls_connection *c)
{
    struct wpabuf *out = c->output;
    c->output = NULL;
    return out ? out : wpabuf_alloc(0);
}

static void export_keys(void *ctx, mbedtls_ssl_key_export_type type,
                        const unsigned char *secret, size_t length,
                        const unsigned char client[32], const unsigned char server[32],
                        mbedtls_tls_prf_types prf)
{
    struct tls_connection *c = ctx;
    if (type != MBEDTLS_SSL_KEY_EXPORT_TLS12_MASTER_SECRET || length != sizeof(c->master)) return;
    memcpy(c->master, secret, length); memcpy(c->random, client, 32);
    memcpy(c->random + 32, server, 32); c->prf = prf; c->exported = true;
}

void *tls_init(const struct tls_config *config) { return os_zalloc(1); }
void tls_deinit(void *ctx) { os_free(ctx); }
int tls_get_errors(void *ctx) { return 0; }
struct tls_connection *tls_connection_init(void *ctx)
{
    struct tls_connection *c = os_zalloc(sizeof(*c));
    if (c) { mbedtls_ssl_init(&c->ssl); mbedtls_ssl_config_init(&c->conf); mbedtls_x509_crt_init(&c->ca); }
    return c;
}
void tls_connection_deinit(void *ctx, struct tls_connection *c)
{
    if (!c) return;
    mbedtls_ssl_free(&c->ssl); mbedtls_ssl_config_free(&c->conf); mbedtls_x509_crt_free(&c->ca);
    wpabuf_clear_free(c->input); wpabuf_clear_free(c->output);
    mbedtls_platform_zeroize(c, sizeof(*c)); os_free(c);
}
int tls_connection_set_params(void *ctx, struct tls_connection *c, const struct tls_connection_params *p)
{
    if (!c || !p || c->configured || p->ca_cert || p->ca_path || p->client_cert ||
        p->private_key || p->client_cert_blob || p->private_key_blob || p->suffix_match ||
        p->subject_match || p->altsubject_match || p->check_cert_subject || p->engine ||
        (p->flags & (TLS_CONN_DISABLE_TLSv1_2 | TLS_CONN_REQUIRE_OCSP | TLS_CONN_REQUIRE_OCSP_ALL))) return -1;
    int ret = mbedtls_ssl_config_defaults(&c->conf, MBEDTLS_SSL_IS_CLIENT,
                                         MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret) return -1;
    mbedtls_ssl_conf_min_tls_version(&c->conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&c->conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_rng(&c->conf, mbedtls_esp_random, NULL);
    // The public AX900 API rejects missing trust material unless explicitly opted out.
    if (p->ca_cert_blob) {
        if (!verification_time_ready()) {
            ESP_LOGW("AX900", "Verified PEAP requires certificate date checks and a set system clock");
            return -1;
        }
        if (!p->domain_match || !p->domain_match[0] ||
            mbedtls_x509_crt_parse(&c->ca, p->ca_cert_blob, p->ca_cert_blob_len) != 0) return -1;
        mbedtls_ssl_conf_ca_chain(&c->conf, &c->ca, NULL);
        mbedtls_ssl_conf_authmode(&c->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
        c->verified = true;
    } else mbedtls_ssl_conf_authmode(&c->conf, MBEDTLS_SSL_VERIFY_NONE);
#if defined(MBEDTLS_SSL_SESSION_TICKETS)
    mbedtls_ssl_conf_session_tickets(&c->conf, MBEDTLS_SSL_SESSION_TICKETS_DISABLED);
#endif
    if (mbedtls_ssl_setup(&c->ssl, &c->conf) ||
        (p->domain_match && mbedtls_ssl_set_hostname(&c->ssl, p->domain_match))) return -1;
    mbedtls_ssl_set_export_keys_cb(&c->ssl, export_keys, c);
    mbedtls_ssl_set_bio(&c->ssl, c, record_write, record_read, NULL);
    c->configured = true;
    return 0;
}
int tls_connection_established(void *ctx, struct tls_connection *c)
{ return c && c->configured && !c->failed && mbedtls_ssl_is_handshake_over(&c->ssl); }
int tls_connection_get_failed(void *ctx, struct tls_connection *c) { return !c || c->failed; }

struct wpabuf *tls_connection_decrypt(void *ctx, struct tls_connection *c, const struct wpabuf *input)
{
    if (!tls_connection_established(ctx, c) || feed(c, input)) return NULL;
    struct wpabuf *plain = wpabuf_alloc(4096);
    if (!plain) return NULL;
    unsigned char chunk[512];
    for (;;) {
        int n = mbedtls_ssl_read(&c->ssl, chunk, sizeof(chunk));
        if (n == MBEDTLS_ERR_SSL_WANT_READ) break;
        if (n <= 0 || (size_t)n > wpabuf_tailroom(plain)) {
            c->failed = true; wpabuf_clear_free(plain); plain = NULL; break;
        }
        wpabuf_put_data(plain, chunk, n);
    }
    mbedtls_platform_zeroize(chunk, sizeof(chunk));
    return plain;
}
struct wpabuf *tls_connection_handshake(void *ctx, struct tls_connection *c,
                                       const struct wpabuf *input, struct wpabuf **application)
{
    if (application) *application = NULL;
    if (!c || !c->configured || feed(c, input)) return NULL;
    if (c->verified && !verification_time_ready()) { c->failed = true; return NULL; }
    int ret = mbedtls_ssl_handshake(&c->ssl);
    if (ret && ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
        c->failed = true;
        ESP_LOGW("AX900", "PEAP TLS failed: -0x%04x", (unsigned)-ret);
    } else if (!ret && application && c->input) {
        *application = tls_connection_decrypt(ctx, c, NULL);
    }
    return take_output(c);
}
struct wpabuf *tls_connection_encrypt(void *ctx, struct tls_connection *c, const struct wpabuf *input)
{
    if (!tls_connection_established(ctx, c) || !input) return NULL;
    int ret = mbedtls_ssl_write(&c->ssl, wpabuf_head(input), wpabuf_len(input));
    if (ret < 0 || (size_t)ret != wpabuf_len(input)) { c->failed = true; return NULL; }
    return take_output(c);
}
int tls_connection_export_key(void *ctx, struct tls_connection *c, const char *label,
                              const u8 *context, size_t context_len, u8 *out, size_t out_len)
{
    if (!tls_connection_established(ctx, c) || !c->exported || context_len > 65535) return -1;
    size_t len = 64 + (context ? 2 + context_len : 0);
    u8 *seed = os_malloc(len);
    if (!seed) return -1;
    memcpy(seed, c->random, 64);
    if (context) { WPA_PUT_BE16(seed + 64, context_len); memcpy(seed + 66, context, context_len); }
    int ret = mbedtls_ssl_tls_prf(c->prf, c->master, sizeof(c->master), label, seed, len, out, out_len);
    os_free(seed); return ret ? -1 : 0;
}
int tls_connection_get_random(void *ctx, struct tls_connection *c, struct tls_random *data)
{
    if (!c || !c->exported) return -1;
    memset(data, 0, sizeof(*data)); data->client_random = c->random; data->client_random_len = 32;
    data->server_random = c->random + 32; data->server_random_len = 32; return 0;
}
int tls_connection_shutdown(void *ctx, struct tls_connection *c)
{
    if (!c || !c->configured) return -1;
    wpabuf_clear_free(c->input); c->input = NULL; c->input_offset = 0;
    wpabuf_clear_free(c->output); c->output = NULL;
    c->failed = c->exported = false; mbedtls_platform_zeroize(c->master, sizeof(c->master));
    return mbedtls_ssl_session_reset(&c->ssl);
}
int tls_connection_resumed(void *ctx, struct tls_connection *c) { return 0; }
int tls_connection_enable_workaround(void *ctx, struct tls_connection *c) { return 0; }
int tls_connection_get_read_alerts(void *ctx, struct tls_connection *c) { return 0; }
int tls_connection_get_write_alerts(void *ctx, struct tls_connection *c) { return 0; }
int tls_get_version(void *ctx, struct tls_connection *c, char *buf, size_t len)
{ if (!c || !c->configured) return -1; os_strlcpy(buf, mbedtls_ssl_get_version(&c->ssl), len); return 0; }
int tls_get_cipher(void *ctx, struct tls_connection *c, char *buf, size_t len)
{ if (!c || !c->configured) return -1; os_strlcpy(buf, mbedtls_ssl_get_ciphersuite(&c->ssl), len); return 0; }
