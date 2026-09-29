// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef void (*ax_frame_message_fn)(void *,uint16_t,const uint8_t *,size_t);
typedef void (*ax_frame_data_fn)(void *,const uint8_t *,size_t);
// AIC USB records may aggregate multiple messages/data records with 4-byte padding.
static inline bool ax_walk_records(const uint8_t *p,size_t left,ax_frame_message_fn fn,ax_frame_data_fn data,void *arg) {
    if(!p || !fn)return false;
    while(left>=4) {
        size_t len=(p[0]|(size_t)p[1]<<8)&0xfff;
        uint8_t type=p[2]&0x7f;
        if(!len && !type) {
            for(size_t i=0;i<left;i++)if(p[i])return false;
            return true;
        }
        size_t raw=len+((type&0x10)?4:60);
        if(raw>left || raw<4)return false;
        if(type==0x11) {
            if(raw<16)return false;
            uint16_t id=p[4]|(uint16_t)p[5]<<8;
            size_t n=p[10]|(size_t)p[11]<<8;
            if(n>raw-16)return false;
            fn(arg,id,p+16,n);
        } else if (!(type & 0x10) && data) {
            data(arg,p,raw);
        }
        size_t step=(raw+3)&~(size_t)3;
        if(step>left)step=raw;
        p+=step;left-=step;
    }
    return left==0;
}
static inline bool ax_walk_messages(const uint8_t *p,size_t left,ax_frame_message_fn fn,void *arg) {
    return ax_walk_records(p,left,fn,NULL,arg);
}
