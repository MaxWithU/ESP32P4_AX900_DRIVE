// SPDX-License-Identifier: Apache-2.0
// Bounded 802.11/SNAP decapsulation, based on the attributed RT-Smart driver.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
typedef void (*ax_ethernet_fn)(void *,const uint8_t *,size_t,bool);
static inline bool ax_snap(const uint8_t *p,size_t n) {
    return n>=8 && p[0]==0xaa && p[1]==0xaa && p[2]==3 && !p[3] && !p[4] && (p[5]==0 || p[5]==0xf8);
}
static inline bool ax_decode_rx(const uint8_t *r,size_t n,uint8_t vif,const uint8_t bssid[6],ax_ethernet_fn deliver,void *ctx) {
    if(!r || n<60 || !deliver)return false;
    size_t len=(r[0]|(size_t)r[1]<<8)&0xfff;
    if(len<24 || len>n-60 || r[49]!=vif || (r[48]&2))return false;
    const uint8_t *f=r+60;
    // Station mode accepts data from its associated AP only.
    if((f[0]&0x0c)!=8 || (f[1]&3)!=2 || memcmp(f+10,bssid,6))return false;
    bool qos=(f[0]&0x80)!=0, encrypted=(f[1]&0x40)!=0;
    size_t header=24+(qos?2:0)+((qos && (f[1]&0x80))?4:0);
    if(header>len || (f[0]&0x40))return false; // null-data has no MSDU
    uint8_t ethernet[1518];
    if(qos && ((r[48]&1) || (f[24]&0x80))) {
        // Locate the first complete A-MSDU subframe, accounting for retained CCMP IV.
        size_t off=header;
        bool found=false;
        const uint8_t ivs[]={0,8};
        for(size_t i=0;i<sizeof(ivs);i++) {
            size_t c=header+ivs[i];
            if(c+22<=len && ax_snap(f+c+14,len-c-14)) {off=c;found=true;break;}
        }
        if(!found)return false;
        // Validate every subframe before delivering any frame from the aggregate.
        for(size_t pass=0;pass<2;pass++) {
            size_t pos=off;
            while(pos<len) {
                if(len-pos<14)return false;
                size_t msdu=(size_t)f[pos+12]<<8|f[pos+13];
                if(msdu<8 || msdu>len-pos-14 || msdu-8>sizeof(ethernet)-14 || !ax_snap(f+pos+14,msdu))return false;
                if(pass){memcpy(ethernet,f+pos,12);memcpy(ethernet+12,f+pos+20,msdu-6);deliver(ctx,ethernet,msdu+6,encrypted);}
                size_t end=pos+14+msdu;if(end==len)break;
                pos+=((14+msdu+3)&~(size_t)3);
                if(pos>len)return false;
            }
        }
        return true;
    }
    const uint8_t ivs[]={0,8};
    for(size_t i=0;i<sizeof(ivs);i++) {
        size_t off=header+ivs[i];
        if(off<=len && ax_snap(f+off,len-off)) {
            size_t payload=len-off-8;if(payload>sizeof(ethernet)-14)return false;
            memcpy(ethernet,f+4,6);memcpy(ethernet+6,f+16,6);
            memcpy(ethernet+12,f+off+6,payload+2);deliver(ctx,ethernet,payload+14,encrypted);return true;
        }
    }
    return false;
}
