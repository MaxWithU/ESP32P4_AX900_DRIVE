#include "ax900_frame.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static const uint8_t *base;
static size_t limit,count;
static uint16_t last_id;
static void receive(void *arg,uint16_t id,const uint8_t *p,size_t n) {
    (void)arg;assert(p>=base);assert((size_t)(p-base)<=limit);assert(n<=limit-(size_t)(p-base));
    count++;last_id=id;
}
static bool parse(const uint8_t *p,size_t n) {
    base=p;limit=n;count=0;return ax_walk_messages(p,n,receive,NULL);
}
int main(void) {
    // Captured chip-revision confirmation from the physical AX900, stage2b.
    const uint8_t chip[]={0x14,0,0x11,0,1,4,0x64,0,1,0,8,0,0x2a,0xde,0xde,0xad,0,0,0x50,0x40,0x20,0x88,7,0xe1};
    assert(parse(chip,sizeof(chip)) && count==1 && last_id==0x401);
    for(size_t n=1;n<sizeof(chip);n++)assert(!parse(chip,n));
    uint8_t aggregate[56]={0};memcpy(aggregate,chip,24);memcpy(aggregate+24,chip,24);
    assert(parse(aggregate,sizeof(aggregate)) && count==2);
    aggregate[10]=9;assert(!parse(aggregate,sizeof(aggregate)) && count==0);
    memcpy(aggregate,chip,24);aggregate[1]=0xf0;
    assert(parse(aggregate,24) && count==1); // upper nibble is USB metadata
    uint8_t data[76]={16,0,0,0};assert(parse(data,sizeof(data)) && count==0);
    // Exercise malformed lengths and aggregate boundaries under ASan/UBSan.
    uint8_t fuzz[512];uint32_t random=0x40500000;
    for(unsigned k=0;k<100000;k++) {
        for(size_t i=0;i<sizeof(fuzz);i++){random=random*1664525+1013904223;fuzz[i]=random>>24;}
        parse(fuzz,k%sizeof(fuzz));
    }
    puts("AX900 captured-frame, truncation, aggregation and 100000 malformed-input checks passed");
}
