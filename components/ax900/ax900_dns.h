// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
static inline uint16_t ax_dns_u16(const uint8_t *p){return (uint16_t)p[0]<<8|p[1];}
static inline size_t ax_dns_question(uint8_t *p,size_t cap,uint16_t id,const char *host) {
    if(cap<18 || !host || !*host)return 0;
    memset(p,0,12);p[0]=id>>8;p[1]=id;p[2]=1;p[5]=1;
    size_t at=12;const char *label=host;
    while(*label){
        const char *end=strchr(label,'.');size_t n=end?(size_t)(end-label):strlen(label);
        if(!n || n>63 || at+1+n+5>cap)return 0;
        p[at++]=n;
        for(size_t i=0;i<n;i++){unsigned char c=label[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'))return 0;p[at++]=c;}
        label=end?end+1:label+n;
    }
    p[at++]=0;p[at++]=0;p[at++]=1;p[at++]=0;p[at++]=1;return at;
}
static inline int ax_dns_skip(const uint8_t *p,size_t n,size_t *at) {
    unsigned labels=0;
    while(*at<n && labels++<128){
        uint8_t len=p[(*at)++];if(!len)return 1;
        if((len&0xc0)==0xc0){if(*at>=n || (((size_t)len&63)<<8|p[*at])>=n)return 0;(*at)++;return 1;}
        if(len>63 || len>n-*at)return 0;*at+=len;
    }return 0;
}
// A/IN responses only; verify exact question and transaction, reject truncation.
// Compression pointers are skipped, never recursively followed.
static inline int ax_dns_answer(const uint8_t *p,size_t n,const uint8_t *q,size_t qn,uint8_t addr[4]) {
    if(qn<17 || n<qn || memcmp(p,q,2) || !(p[2]&0x80) || (p[2]&0x7a) ||
       (p[3]&15) || ax_dns_u16(p+4)!=1 || memcmp(p+12,q+12,qn-12))return 0;
    size_t at=qn;unsigned answers=ax_dns_u16(p+6);
    if(answers>128)return 0;
    for(unsigned i=0;i<answers;i++){
        if(!ax_dns_skip(p,n,&at) || n-at<10)return 0;
        uint16_t type=ax_dns_u16(p+at),cls=ax_dns_u16(p+at+2),len=ax_dns_u16(p+at+8);at+=10;
        if(len>n-at)return 0;
        if(type==1 && cls==1 && len==4){memcpy(addr,p+at,4);return 1;}at+=len;
    }return 0;
}
