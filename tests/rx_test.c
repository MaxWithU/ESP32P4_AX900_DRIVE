#include "ax900_rx.h"
#include <assert.h>
#include <stdio.h>
static unsigned count;
static uint8_t expected[64];
static size_t expected_len;
static void receive(void *ctx,const uint8_t *data,size_t len,bool encrypted) {
    (void)ctx;(void)encrypted;assert(len==expected_len);assert(!memcmp(data,expected,len));count++;
}
static void fuzz_receive(void *ctx,const uint8_t *data,size_t len,bool encrypted) {
    (void)ctx;(void)encrypted;assert(data && len>=14 && len<=1518);
    volatile uint8_t last=data[len-1];(void)last;
}
int main(void) {
    const uint8_t bssid[6]={2,3,4,5,6,7};
    uint8_t record[128]={0};record[0]=36;record[49]=1;
    uint8_t *f=record+60;f[0]=0x08;f[1]=0x02;
    memset(f+4,0x10,6);memcpy(f+10,bssid,6);memset(f+16,0x20,6);
    const uint8_t snap[12]={0xaa,0xaa,3,0,0,0,0x88,0x8e,1,2,3,4};memcpy(f+24,snap,12);
    memset(expected,0x10,6);memset(expected+6,0x20,6);memcpy(expected+12,snap+6,6);expected_len=18;
    assert(ax_decode_rx(record,96,1,bssid,receive,NULL) && count==1);
    for(size_t n=0;n<96;n++)assert(!ax_decode_rx(record,n,1,bssid,receive,NULL));
    assert(!ax_decode_rx(record,96,2,bssid,receive,NULL));
    record[48]=2;assert(!ax_decode_rx(record,96,1,bssid,receive,NULL));record[48]=0;
    f[10]^=1;assert(!ax_decode_rx(record,96,1,bssid,receive,NULL));f[10]^=1;
    // QoS + retained CCMP IV: SNAP begins after the 26-byte header and eight-byte IV.
    memmove(f+34,f+24,12);memset(f+24,0,10);f[0]=0x88;f[1]=0x42;record[0]=46;
    assert(ax_decode_rx(record,106,1,bssid,receive,NULL) && count==2);
    f[0]=0xc8;assert(!ax_decode_rx(record,106,1,bssid,receive,NULL));
    // One complete A-MSDU subframe; malformed lengths must not be delivered.
    memset(f,0,68);f[0]=0x88;f[1]=2;memcpy(f+10,bssid,6);f[24]=0x80;
    memcpy(f+26,expected,12);f[38]=0;f[39]=12;memcpy(f+40,snap,12);record[0]=52;
    assert(ax_decode_rx(record,112,1,bssid,receive,NULL) && count==3);
    f[39]=13;assert(!ax_decode_rx(record,112,1,bssid,receive,NULL) && count==3);
    // Exercise both early rejection and deep header/aggregate parsing under ASan/UBSan.
    uint32_t seed=42;for(unsigned i=0;i<100000;i++) {
        for(size_t k=0;k<sizeof(record);k++){seed=seed*1664525+1013904223;record[k]=seed>>24;}
        if(i%2){
            record[0]=24+(seed%45);record[1]=0;record[49]=1;record[48]&=1;
            record[60]=(record[60]&0x80)|8;record[61]=(record[61]&0xc0)|2;
            memcpy(record+70,bssid,6);
        }
        ax_decode_rx(record,i%3?sizeof(record):i%sizeof(record),1,bssid,fuzz_receive,NULL);
    }
    puts("RX bounds, QoS, CCMP IV, A-MSDU and malformed-record checks passed");
}
