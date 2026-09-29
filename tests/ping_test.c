// SPDX-License-Identifier: Apache-2.0
// Compile the complete production ping implementation, with deterministic I/O.
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include "lwip/test_lwip.h"
static int64_t now_us,step_us,receive_timeout;
static unsigned reads,packets;
static uint8_t wire[80];
static size_t wire_len;
static uint32_t sender;
static int family;
static int fake_setsockopt(int,int,int,const void *,socklen_t);
static ssize_t fake_recvfrom(int,void *,size_t,int,struct sockaddr *,socklen_t *);
#define setsockopt fake_setsockopt
#define recvfrom fake_recvfrom
#include "../components/ax900/ax900_ping.c"
#undef setsockopt
#undef recvfrom
static esp_ping_t *active;
static bool stop_on_receive;
int64_t esp_timer_get_time(void){return now_us;}
static int fake_setsockopt(int fd,int level,int option,const void *value,socklen_t size){
    (void)fd;
    if(level==SOL_SOCKET && option==SO_RCVTIMEO){
        assert(size==sizeof(struct timeval));const struct timeval *t=value;
        receive_timeout=(int64_t)t->tv_sec*1000000+t->tv_usec;
        assert(receive_timeout>0 && receive_timeout<=100000);
    }
    return 0;
}
static ssize_t fake_recvfrom(int fd,void *buf,size_t capacity,int flags,struct sockaddr *from,socklen_t *len){
    (void)fd;assert(!flags && *len==sizeof(struct sockaddr_storage));reads++;
    if(stop_on_receive)assert(esp_ping_stop(active)==ESP_OK);
    if(!packets || step_us>receive_timeout){now_us+=receive_timeout;errno=EAGAIN;return -1;}
    packets--;now_us+=step_us;
    memset(from,0,*len);from->sa_family=family;
    if(family==AF_INET)((struct sockaddr_in *)from)->sin_addr.s_addr=sender;
    *len=sizeof(struct sockaddr_in);
    size_t n=wire_len<capacity?wire_len:capacity;memcpy(buf,wire,n);return n;
}
static esp_ping_t setup(unsigned ihl,size_t len){
    static struct icmp_echo_hdr sent={.id=0x1234,.seqno=0x5678};
    esp_ping_t ep={.packet_hdr=&sent,.timeout_ms=1500,.flags=PING_FLAGS_INIT|PING_FLAGS_START};
    struct sockaddr_in *target=(struct sockaddr_in *)&ep.target_addr;
    target->sin_family=AF_INET;target->sin_addr.s_addr=htonl(0xc0000201);
    now_us=0;step_us=100000;reads=0;packets=1;wire_len=len;family=AF_INET;sender=target->sin_addr.s_addr;stop_on_receive=false;
    memset(wire,0,sizeof(wire));wire[0]=0x40|ihl;wire[2]=0;wire[3]=(uint8_t)len;wire[8]=64;wire[9]=1;
    if(ihl>=5){memcpy(wire+ihl*4+4,&sent.id,2);memcpy(wire+ihl*4+6,&sent.seqno,2);}
    return ep;
}
int main(void){
    esp_ping_t ep=setup(15,64);assert(esp_ping_receive(&ep)<0 && !ep.received);
    ep=setup(15,68);assert(esp_ping_receive(&ep)==68 && ep.received==1 && !ep.recv_len);
    ep=setup(6,40);assert(esp_ping_receive(&ep)==40 && ep.recv_len==8 && ep.ttl==64);
    ep=setup(5,80);assert(esp_ping_receive(&ep)==68 && ep.recv_len==52); // truncated payload is allowed
    for(unsigned ihl=0;ihl<16;ihl++)for(size_t n=0;n<=68;n++){
        ep=setup(ihl,n);int rc=esp_ping_receive(&ep);
        assert((rc>0)==(ihl>=5 && n>=ihl*4+8));
    }
    ep=setup(5,28);wire[3]=27;assert(esp_ping_receive(&ep)<0 && !ep.received);
    ep=setup(5,28);wire[0]=0x65;assert(esp_ping_receive(&ep)<0);
    ep=setup(5,28);wire[9]=17;assert(esp_ping_receive(&ep)<0);
    ep=setup(5,28);wire[20]=ICMP_ECHO;assert(esp_ping_receive(&ep)<0);
    ep=setup(5,28);wire[21]=1;assert(esp_ping_receive(&ep)<0);
    ep=setup(5,28);sender++;assert(esp_ping_receive(&ep)<0);
    ep=setup(5,28);family=AF_UNSPEC;assert(esp_ping_receive(&ep)<0);
    ep=setup(5,28);wire[24]^=1;packets=100;
    assert(esp_ping_receive(&ep)<0 && reads==15 && now_us==1500000 && packets==85);
    ep=setup(5,28);packets=0;assert(esp_ping_receive(&ep)<0 && now_us==1500000);
    ep=setup(5,28);active=&ep;stop_on_receive=true;assert(esp_ping_receive(&ep)<0 && reads==1 && !ep.received);
    ep=setup(5,28);ep.flags=PING_FLAGS_INIT;assert(esp_ping_receive(&ep)<0 && !reads);
    ep=setup(5,28);ep.timeout_ms=1550;packets=100;wire[24]^=1;
    assert(esp_ping_receive(&ep)<0 && now_us==1550000 && receive_timeout==50000);
    esp_ping_config_t invalid={0};esp_ping_handle_t handle=NULL;
    assert(esp_ping_new_session(&invalid,NULL,&handle)==ESP_ERR_INVALID_ARG && !handle);
    puts("Production ping: IPv4 options/bounds, reply filtering, total deadline and cancellation passed");
}
