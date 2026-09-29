// SPDX-License-Identifier: Apache-2.0
#include "ax900_dns.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    uint8_t q[272],p[512],ip[4];size_t n=ax_dns_question(q,sizeof(q),0x1234,"example.test");
    assert(n==30 && q[12]==7 && q[20]==4);
    memcpy(p,q,n);p[2]=0x81;p[3]=0x80;p[7]=1;
    const uint8_t answer[]={0xc0,0x0c,0,1,0,1,0,0,0,60,0,4,192,0,2,1};
    memcpy(p+n,answer,sizeof(answer));size_t size=n+sizeof(answer);
    assert(ax_dns_answer(p,size,q,n,ip) && !memcmp(ip,"\xc0\0\2\1",4));
    for(size_t i=0;i<size;i++)assert(!ax_dns_answer(p,i,q,n,ip));
    p[0]++;assert(!ax_dns_answer(p,size,q,n,ip));p[0]--;
    p[2]|=2;assert(!ax_dns_answer(p,size,q,n,ip));p[2]&=~2;
    p[3]|=3;assert(!ax_dns_answer(p,size,q,n,ip));p[3]&=~3;
    p[n+1]=0xff;p[n]=0xff;assert(!ax_dns_answer(p,size,q,n,ip));
    assert(!ax_dns_question(q,sizeof(q),1,"bad..test"));
    assert(!ax_dns_question(q,sizeof(q),1,"x\r\ntest"));
    assert(!ax_dns_question(q,15,1,"example.test"));
    puts("DNS packet bounds, transaction and question validation passed");
}
