// SPDX-License-Identifier: Apache-2.0
// Exercises the production TLS bridge against a real in-memory mbedTLS server.
#include "includes.h"
#include "common.h"
#include "crypto/tls.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/pk.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/platform_time.h"
#include "mbedtls/esp_mbedtls_random.h"
#include <assert.h>
#include "ax900_issue.h"
static mbedtls_time_t test_now;
static mbedtls_time_t test_time(mbedtls_time_t *out){if(out)*out=test_now;return test_now;}

void *os_zalloc(size_t n) { return calloc(1,n); }
void *os_memdup(const void *p,size_t n) { void *out=malloc(n); if(out)memcpy(out,p,n); return out; }
size_t os_strlcpy(char *dst,const char *src,size_t len) { size_t n=strlen(src); if(len){size_t copy=n<len-1?n:len-1;memcpy(dst,src,copy);dst[copy]=0;}return n; }

int hexstr2bin(const char *text,u8 *out,size_t len) {
    for(size_t i=0;i<len;i++){unsigned n;if(sscanf(text+2*i,"%2x",&n)!=1)return -1;out[i]=n;}return 0;
}
struct server {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cert;
    mbedtls_pk_context key;
    struct wpabuf *input,*output;
    size_t offset;
    unsigned char master[48],random[64];
    mbedtls_tls_prf_types prf;
};
static int read_record(void *ctx,unsigned char *buf,size_t len) {
    struct server *s=ctx;
    if(!s->input)return MBEDTLS_ERR_SSL_WANT_READ;
    size_t available=wpabuf_len(s->input)-s->offset;
    if(len>available)len=available;
    memcpy(buf,wpabuf_head_u8(s->input)+s->offset,len);s->offset+=len;
    if(s->offset==wpabuf_len(s->input)){wpabuf_free(s->input);s->input=NULL;s->offset=0;}
    return (int)len;
}
static int write_record(void *ctx,const unsigned char *buf,size_t len) {
    struct server *s=ctx;assert(wpabuf_resize(&s->output,len)==0);
    wpabuf_put_data(s->output,buf,len);return (int)len;
}
static void exported(void *ctx,mbedtls_ssl_key_export_type type,const unsigned char *secret,size_t len,
                     const unsigned char client[32],const unsigned char server[32],mbedtls_tls_prf_types prf) {
    struct server *s=ctx;assert(type==MBEDTLS_SSL_KEY_EXPORT_TLS12_MASTER_SECRET && len==48);
    memcpy(s->master,secret,48);memcpy(s->random,client,32);memcpy(s->random+32,server,32);s->prf=prf;
}
static void deliver_server(struct server *s,struct wpabuf *data) {
    if(data && wpabuf_len(data)){
        assert(!s->input || s->offset==0);
        assert(wpabuf_resize(&s->input,wpabuf_len(data))==0);wpabuf_put_buf(s->input,data);
    }
    wpabuf_free(data);
}
static unsigned char *load(const char *path,size_t *len) {
    FILE *f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);*len=ftell(f)+1;rewind(f);
    unsigned char *data=calloc(1,*len);assert(data && fread(data,1,*len-1,f)==*len-1);fclose(f);return data;
}
static void run(const unsigned char *cert,size_t cert_len,const char *cert_path,const char *key_path,
                const char *name,bool trusted,bool expected_success) {
    ax_issue_set(AX900_ISSUE_NONE);
    struct server s={0};
    mbedtls_ssl_init(&s.ssl);mbedtls_ssl_config_init(&s.conf);mbedtls_x509_crt_init(&s.cert);mbedtls_pk_init(&s.key);
    assert(mbedtls_x509_crt_parse_file(&s.cert,cert_path)==0);
    assert(mbedtls_pk_parse_keyfile(&s.key,key_path,NULL,mbedtls_esp_random,NULL)==0);
    assert(mbedtls_ssl_config_defaults(&s.conf,MBEDTLS_SSL_IS_SERVER,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT)==0);
    mbedtls_ssl_conf_min_tls_version(&s.conf,MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&s.conf,MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_rng(&s.conf,mbedtls_esp_random,NULL);
    mbedtls_ssl_conf_authmode(&s.conf,MBEDTLS_SSL_VERIFY_NONE);
    assert(mbedtls_ssl_conf_own_cert(&s.conf,&s.cert,&s.key)==0);
    assert(mbedtls_ssl_setup(&s.ssl,&s.conf)==0);
    mbedtls_ssl_set_bio(&s.ssl,&s,write_record,read_record,NULL);mbedtls_ssl_set_export_keys_cb(&s.ssl,exported,&s);
    void *ctx=tls_init(NULL);struct tls_connection *client=tls_connection_init(ctx);assert(client);
    struct tls_connection_params config={0};
    if(trusted){config.ca_cert_blob=cert;config.ca_cert_blob_len=cert_len;config.domain_match=name;}
    assert(tls_connection_set_params(ctx,client,&config)==0);
    deliver_server(&s,tls_connection_handshake(ctx,client,NULL,NULL));
    for(unsigned round=0;round<200 && !tls_connection_get_failed(ctx,client);round++) {
        int ret=mbedtls_ssl_handshake(&s.ssl);
        assert(ret==0 || ret==MBEDTLS_ERR_SSL_WANT_READ);
        struct wpabuf *out=s.output;s.output=NULL;
        // Fragment TLS records at arbitrary non-record boundaries.
        if(out)for(size_t off=0;off<wpabuf_len(out);) {
            size_t len=wpabuf_len(out)-off;if(len>37)len=37;
            struct wpabuf *fragment=wpabuf_alloc_copy(wpabuf_head_u8(out)+off,len);
            deliver_server(&s,tls_connection_handshake(ctx,client,fragment,NULL));wpabuf_free(fragment);off+=len;
            if(tls_connection_get_failed(ctx,client))break;
        }
        wpabuf_free(out);
        if(tls_connection_established(ctx,client) && mbedtls_ssl_is_handshake_over(&s.ssl))break;
    }
    assert(!!tls_connection_established(ctx,client)==expected_success);
    if(expected_success) {
        unsigned char client_key[64],server_key[64];
        assert(tls_connection_export_key(ctx,client,"client EAP encryption",NULL,0,client_key,64)==0);
        assert(mbedtls_ssl_tls_prf(s.prf,s.master,48,"client EAP encryption",s.random,64,server_key,64)==0);
        assert(!memcmp(client_key,server_key,64));
        const unsigned char payload[]={1,0,0,0xff,3,42,0x80};
        assert(mbedtls_ssl_write(&s.ssl,payload,sizeof(payload))==sizeof(payload));
        struct wpabuf *plain=tls_connection_decrypt(ctx,client,s.output);
        assert(plain && wpabuf_len(plain)==sizeof(payload) && !memcmp(wpabuf_head(plain),payload,sizeof(payload)));
        wpabuf_clear_free(plain);wpabuf_free(s.output);s.output=NULL;
        plain=wpabuf_alloc_copy(payload,sizeof(payload));deliver_server(&s,tls_connection_encrypt(ctx,client,plain));wpabuf_free(plain);
        unsigned char received[32];assert(mbedtls_ssl_read(&s.ssl,received,sizeof(received))==sizeof(payload));
        assert(!memcmp(received,payload,sizeof(payload)));
    } else {
        assert(tls_connection_get_failed(ctx,client));
        ax900_issue_t issue=ax900_get_connection_issue();
        if(strstr(cert_path,"expired") || strstr(cert_path,"future"))assert(issue==AX900_ISSUE_CERT_DATE);
        else if(name && !strcmp(name,"wrong.test"))assert(issue==AX900_ISSUE_CERT_NAME);
        else assert(issue==AX900_ISSUE_CERTIFICATE);
    }
    tls_connection_deinit(ctx,client);tls_deinit(ctx);
    mbedtls_ssl_free(&s.ssl);mbedtls_ssl_config_free(&s.conf);mbedtls_x509_crt_free(&s.cert);mbedtls_pk_free(&s.key);
    wpabuf_free(s.input);wpabuf_free(s.output);
}
int main(int argc,char **argv) {
    assert(argc==6);size_t len,other_len;unsigned char *cert=load(argv[1],&len),*other=load(argv[3],&other_len);
    test_now=time(NULL);assert(mbedtls_platform_set_time(test_time)==0);
    struct tls_connection_params p={.ca_cert_blob=cert,.ca_cert_blob_len=len,.domain_match="radius.test"};
    void *ctx=tls_init(NULL);struct tls_connection *c=tls_connection_init(ctx);assert(c);
    mbedtls_time_t saved=test_now;test_now=0;
    assert(tls_connection_set_params(ctx,c,&p)==-1);
#if defined(MBEDTLS_HAVE_TIME_DATE)
    assert(ax900_get_connection_issue()==AX900_ISSUE_CLOCK);
#else
    assert(ax900_get_connection_issue()==AX900_ISSUE_TLS_CONFIG);
#endif
    tls_connection_deinit(ctx,c);test_now=saved;
#if defined(MBEDTLS_HAVE_TIME_DATE)
    c=tls_connection_init(ctx);assert(tls_connection_set_params(ctx,c,&p)==0);
    test_now=0;assert(tls_connection_handshake(ctx,c,NULL,NULL)==NULL && tls_connection_get_failed(ctx,c));
    tls_connection_deinit(ctx,c);test_now=saved;
    run(cert,len,argv[1],argv[2],"radius.test",true,true);
    run(cert,len,argv[1],argv[2],"wrong.test",true,false);
    run(other,other_len,argv[1],argv[2],"radius.test",true,false);
    run(cert,len,argv[4],argv[2],"radius.test",true,false); // expired, trusted issuer/name
    run(cert,len,argv[5],argv[2],"radius.test",true,false); // not yet valid
#else
    c=tls_connection_init(ctx);assert(tls_connection_set_params(ctx,c,&p)==-1);
    tls_connection_deinit(ctx,c);
#endif
    tls_deinit(ctx);
    run(cert,len,argv[1],argv[2],NULL,false,true);
    test_now=0;run(cert,len,argv[1],argv[2],NULL,false,true);
    free(cert);free(other);
    puts("TLS bridge: configured date policy, unset/reset clock rejection and explicit no-validation mode passed");
}
