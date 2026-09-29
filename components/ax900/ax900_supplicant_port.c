// SPDX-License-Identifier: Apache-2.0
// OS, cooperative timers and mbedTLS primitives for the unmodified hostap core.
#include "includes.h"
#include "common.h"
#include "eloop.h"
#include "crypto/crypto.h"
#include "crypto/sha1.h"
#include "crypto/sha256.h"
#include "crypto/md5.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "mbedtls/md.h"
#include "mbedtls/aes.h"

void *os_zalloc(size_t n) { return calloc(1,n); }
void *os_memdup(const void *p,size_t n) { void *q=malloc(n);if(q)memcpy(q,p,n);return q; }
int os_get_random(unsigned char *p,size_t n) { esp_fill_random(p,n);return 0; }
int os_get_reltime(struct os_reltime *t) { int64_t n=esp_timer_get_time();t->sec=n/1000000;t->usec=n%1000000;return 0; }
int os_get_time(struct os_time *t) { struct timeval v;gettimeofday(&v,NULL);t->sec=v.tv_sec;t->usec=v.tv_usec;return 0; }
struct ax_timeout { eloop_timeout_handler fn;void *ctx,*user;int64_t deadline; };
static struct ax_timeout timers[16];
int eloop_register_timeout(unsigned s,unsigned us,eloop_timeout_handler fn,void *ctx,void *user) {
    for(size_t i=0;i<16;i++)if(!timers[i].fn) {timers[i]=(struct ax_timeout){fn,ctx,user,esp_timer_get_time()+(int64_t)s*1000000+us};return 0;}
    return -1;
}
int eloop_cancel_timeout(eloop_timeout_handler fn,void *ctx,void *user) {
    int n=0;for(size_t i=0;i<16;i++)if(timers[i].fn==fn && (ctx==ELOOP_ALL_CTX || timers[i].ctx==ctx) && (user==ELOOP_ALL_CTX || timers[i].user==user)){timers[i].fn=NULL;n++;}return n;
}
int eloop_is_timeout_registered(eloop_timeout_handler fn,void *ctx,void *user) {
    for(size_t i=0;i<16;i++)if(timers[i].fn==fn && timers[i].ctx==ctx && timers[i].user==user)return 1;return 0;
}
void ax_supplicant_poll(void) {
    int64_t now=esp_timer_get_time();for(size_t i=0;i<16;i++)if(timers[i].fn && timers[i].deadline<=now){struct ax_timeout t=timers[i];timers[i].fn=NULL;t.fn(t.ctx,t.user);}
}
static int digest(mbedtls_md_type_t type,const u8 *key,size_t keylen,size_t count,const u8 *addr[],const size_t *len,u8 *out) {
    mbedtls_md_context_t c;mbedtls_md_init(&c);const mbedtls_md_info_t *info=mbedtls_md_info_from_type(type);
    int r=info?mbedtls_md_setup(&c,info,key!=NULL):-1;
    if(!r)r=key?mbedtls_md_hmac_starts(&c,key,keylen):mbedtls_md_starts(&c);
    for(size_t i=0;!r && i<count;i++)r=key?mbedtls_md_hmac_update(&c,addr[i],len[i]):mbedtls_md_update(&c,addr[i],len[i]);
    if(!r)r=key?mbedtls_md_hmac_finish(&c,out):mbedtls_md_finish(&c,out);
    mbedtls_md_free(&c);return r? -1:0;
}
#define AX_DIGEST(name,type) \
int name##_vector(size_t n,const u8 *p[],const size_t *l,u8 *out){return digest(type,NULL,0,n,p,l,out);} \
int hmac_##name##_vector(const u8 *key,size_t keylen,size_t n,const u8 *p[],const size_t *l,u8 *out){return digest(type,key,keylen,n,p,l,out);} \
int hmac_##name(const u8 *key,size_t keylen,const u8 *p,size_t n,u8 *out){return digest(type,key,keylen,1,&p,&n,out);}
AX_DIGEST(sha1,MBEDTLS_MD_SHA1)
AX_DIGEST(sha256,MBEDTLS_MD_SHA256)
AX_DIGEST(md5,MBEDTLS_MD_MD5)
void *aes_encrypt_init(const u8 *key,size_t len) {mbedtls_aes_context *c=calloc(1,sizeof(*c));if(!c)return NULL;mbedtls_aes_init(c);if(mbedtls_aes_setkey_enc(c,key,len*8)){mbedtls_aes_free(c);free(c);return NULL;}return c;}
int aes_encrypt(void *c,const u8 *in,u8 *out){return mbedtls_aes_crypt_ecb(c,MBEDTLS_AES_ENCRYPT,in,out);}
void aes_encrypt_deinit(void *c){if(c){mbedtls_aes_free(c);free(c);}}
void *aes_decrypt_init(const u8 *key,size_t len) {mbedtls_aes_context *c=calloc(1,sizeof(*c));if(!c)return NULL;mbedtls_aes_init(c);if(mbedtls_aes_setkey_dec(c,key,len*8)){mbedtls_aes_free(c);free(c);return NULL;}return c;}
int aes_decrypt(void *c,const u8 *in,u8 *out){return mbedtls_aes_crypt_ecb(c,MBEDTLS_AES_DECRYPT,in,out);}
void aes_decrypt_deinit(void *c){if(c){mbedtls_aes_free(c);free(c);}}
int os_memcmp_const(const void *a,const void *b,size_t n) {
    const u8 *x=a,*y=b;volatile u8 difference=0;
    for(size_t i=0;i<n;i++)difference|=x[i]^y[i];return difference;
}

size_t os_strlcpy(char *dst,const char *src,size_t size) {
    size_t len=strlen(src);
    if(size){size_t n=len<size-1?len:size-1;memcpy(dst,src,n);dst[n]=0;}
    return len;
}

// This station has no roaming scan cache or preauthentication transport.
// A fresh EAP exchange is required after every association.
struct wpa_sm;
void rsn_preauth_candidate_process(struct wpa_sm *sm) {}
void rsn_preauth_deinit(struct wpa_sm *sm) {}
