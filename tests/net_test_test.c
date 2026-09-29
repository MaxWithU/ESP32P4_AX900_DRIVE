// SPDX-License-Identifier: Apache-2.0
// Public test API against a deterministic, synthetic socket peer. No real network.
#include <assert.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <net/if.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "lwip/sockets.h"
#include "ax900.h"
#include "esp_netif.h"
#include "esp_heap_caps.h"
static bool online=true,fail_task,fail_bind,timeout_peer;
static uint32_t epoch=1;
static int64_t clock_us;
static int opens,closes,bound_device,bound_source;
static int dns_reads;
static void (*worker)(void *);static void *worker_arg;
static const char *response;static size_t response_at;
static int fake_socket(int domain,int type,int protocol){assert(domain==AF_INET && (type==SOCK_STREAM || type==SOCK_DGRAM) && !protocol);opens++;return 9;}
static int fake_setsockopt(int fd,int level,int option,const void *value,socklen_t n){assert(fd==9 && level==SOL_SOCKET && option==SO_BINDTODEVICE && n==sizeof(struct ifreq));assert(!strcmp(((const struct ifreq *)value)->ifr_name,"ax0"));bound_device++;return fail_bind?-1:0;}
static int fake_bind(int fd,const struct sockaddr *addr,socklen_t n){assert(fd==9 && n==sizeof(struct sockaddr_in));assert(((const struct sockaddr_in *)addr)->sin_addr.s_addr==htonl(0xc0000201));bound_source++;return 0;}
static int fake_fcntl(int fd,int command,int flags){assert(fd==9 && command==F_SETFL && flags==O_NONBLOCK);return 0;}
static int fake_connect(int fd,const struct sockaddr *addr,socklen_t n){assert(fd==9 && n==sizeof(struct sockaddr_in));(void)addr;return 0;}
static ssize_t fake_send(int fd,const void *data,size_t n,int flags){assert(fd==9 && !flags && data);return n;}
static ssize_t fake_recv(int fd,void *data,size_t n,int flags){assert(fd==9 && !flags);if(timeout_peer){errno=EAGAIN;return -1;}if(!response || !response[response_at])return 0;((char *)data)[0]=response[response_at++];return n?1:0;}
static int fake_close(int fd){assert(fd==9);closes++;return 0;}
static int fake_select(int n,fd_set *r,fd_set *w,fd_set *e,struct timeval *t){assert(n==10);(void)r;(void)w;(void)e;clock_us+=t->tv_sec*1000000+t->tv_usec;return timeout_peer?0:1;}
static char *fake_ifname(unsigned index,char *name){assert(index==7);strcpy(name,"ax0");return name;}
#define socket fake_socket
#define setsockopt fake_setsockopt
#define bind fake_bind
#define fcntl fake_fcntl
#define connect fake_connect
#define send fake_send
#define recv fake_recv
#define close fake_close
#define select fake_select
#define if_indextoname fake_ifname
#include "../components/ax900/ax900_test.c"
int64_t esp_timer_get_time(void){return clock_us;}
uint32_t esp_random(void){return 0x12345678;}
size_t heap_caps_get_free_size(unsigned c){(void)c;return 64000;}
size_t heap_caps_get_minimum_free_size(unsigned c){(void)c;return 32000;}
int xTaskCreate(void (*fn)(void *),const char *name,unsigned stack,void *arg,unsigned priority,void *handle){(void)name;(void)stack;(void)priority;(void)handle;if(fail_task)return 0;worker=fn;worker_arg=arg;return 1;}
void vTaskDelete(void *p){assert(!p);}
void vTaskDelay(unsigned n){clock_us+=(int64_t)n*1000;}
bool ax900_connection_is_current(uint32_t id){return online && id==epoch;}
void ax900_get_link_status(ax900_link_status_t *s){memset(s,0,sizeof(*s));s->has_ip=s->authenticated=online;s->connection_id=epoch;}
static esp_netif_t iface;
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key){assert(online && !strcmp(key,"AX900"));return &iface;}
esp_err_t esp_netif_get_ip_info(esp_netif_t *n,esp_netif_ip_info_t *ip){assert(n==&iface);memset(ip,0,sizeof(*ip));ip->ip.addr=htonl(0xc0000201);return ESP_OK;}
int esp_netif_get_netif_impl_index(esp_netif_t *n){assert(n==&iface);return 7;}
esp_err_t esp_netif_get_dns_info(esp_netif_t *n,int type,esp_netif_dns_info_t *out){assert(n==&iface && !type);dns_reads++;memset(out,0,sizeof(*out));out->ip.u_addr.ip4.addr=htonl(CONFIG_ESP_NETIF_SET_DNS_PER_DEFAULT_NETIF?0xc0000235:0xc6336435);return ESP_OK;}
static ax900_test_result_t result(void){ax900_test_result_t r;ax900_test_get_result(&r);return r;}
static void finish(void){assert(worker);worker(worker_arg);worker=NULL;assert(opens==closes);}
int main(void){
    ax900_test_config_t c={.kind=AX900_TEST_TCP,.host="192.0.2.2",.port=5201,.timeout_ms=1000,.bytes=1024,.path="/health"};
    assert(ax900_test_start(NULL)==ESP_ERR_INVALID_ARG);
    ax900_test_config_t bad=c;strcpy(bad.host,"x\r\nHeader: bad");assert(ax900_test_start(&bad)==ESP_ERR_INVALID_ARG);
    bad=c;memset(bad.host,'x',sizeof(bad.host));assert(ax900_test_start(&bad)==ESP_ERR_INVALID_ARG);
    bad=c;bad.kind=AX900_TEST_HTTP;strcpy(bad.path,"/\r\n");assert(ax900_test_start(&bad)==ESP_ERR_INVALID_ARG);
    bad=c;bad.kind=AX900_TEST_UPLOAD;bad.bytes=8388609;assert(ax900_test_start(&bad)==ESP_ERR_INVALID_ARG);
    online=false;assert(ax900_test_start(&c)==ESP_ERR_INVALID_STATE);online=true;
    fail_task=true;assert(ax900_test_start(&c)==ESP_ERR_NO_MEM);fail_task=false;
    assert(ax900_test_start(&c)==ESP_OK);assert(ax900_test_start(&c)==ESP_ERR_INVALID_STATE);finish();
    assert(result().state==AX900_TEST_PASSED && bound_device==1 && bound_source==1);
    ax900_test_config_t named=c;strcpy(named.host,"peer.test");
#if CONFIG_ESP_NETIF_SET_DNS_PER_DEFAULT_NETIF
    assert(ax900_test_start(&named)==ESP_OK);
    assert(((struct test *)worker_arg)->dns==htonl(0xc0000235) && dns_reads>0);
    ax900_test_cancel();finish();
#else
    assert(ax900_test_start(&named)==ESP_ERR_NOT_SUPPORTED && dns_reads==0);
#endif
    strcpy(named.dns_ipv4,"203.0.113.53");assert(ax900_test_start(&named)==ESP_OK);
    assert(((struct test *)worker_arg)->dns==htonl(0xcb007135));ax900_test_cancel();finish();
    assert(ax900_test_start(&c)==ESP_OK);ax900_test_cancel();finish();assert(result().state==AX900_TEST_CANCELLED);
    assert(ax900_test_start(&c)==ESP_OK);epoch++;finish();assert(result().state==AX900_TEST_STALE);
    fail_bind=true;assert(ax900_test_start(&c)==ESP_OK);finish();assert(result().state==AX900_TEST_FAILED);fail_bind=false;
    c.kind=AX900_TEST_HTTP;
    response="HTTP/1.1 204 No Content\r\n";response_at=0;
    assert(ax900_test_start(&c)==ESP_OK);finish();assert(result().state==AX900_TEST_PASSED && result().http_status==204);
    response="HTTP/1.1 500 Error\r\n";response_at=0;
    assert(ax900_test_start(&c)==ESP_OK);finish();assert(result().state==AX900_TEST_FAILED && result().http_status==500);
    response="HTTP/1.1 103 Early Hints\r\nLink: </style.css>; rel=preload\r\n\r\nHTTP/1.1 500 Error\r\n";response_at=0;
    assert(ax900_test_start(&c)==ESP_OK);finish();assert(result().state==AX900_TEST_FAILED && result().http_status==500);
    response="HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 103 Early Hints\r\n\r\nHTTP/1.1 204 No Content\r\n";response_at=0;
    assert(ax900_test_start(&c)==ESP_OK);finish();assert(result().state==AX900_TEST_PASSED && result().http_status==204);
    const char *incomplete[]={"HTTP/1.1 103 Early Hints\r\n\r\n", "HTTP/1.1 103 Early Hints\r\nX: unfinished", "HTTP/1.1 101 Switching Protocols\r\n\r\n", "HTTP/1.1 200 OK\n"};
    for(size_t i=0;i<sizeof(incomplete)/sizeof(incomplete[0]);i++){
        response=incomplete[i];response_at=0;assert(ax900_test_start(&c)==ESP_OK);finish();assert(result().state==AX900_TEST_FAILED && !result().http_status);
    }
    char excessive[1024]="";
    for(unsigned i=0;i<10;i++)strcat(excessive,"HTTP/1.1 103 Early Hints\r\n\r\n");
    strcat(excessive,"HTTP/1.1 204 No Content\r\n");response=excessive;response_at=0;
    assert(ax900_test_start(&c)==ESP_OK);finish();assert(result().state==AX900_TEST_FAILED && !result().http_status);
    response="HTTP/1.1 2000 Bogus\r\n";response_at=0;
    assert(ax900_test_start(&c)==ESP_OK);finish();assert(result().state==AX900_TEST_FAILED && !result().http_status);
    response="HTTP/1.1 200 OK";response_at=0;
    assert(ax900_test_start(&c)==ESP_OK);finish();assert(result().state==AX900_TEST_FAILED);
    timeout_peer=true;assert(ax900_test_start(&c)==ESP_OK);finish();assert(result().error==ESP_ERR_TIMEOUT);
    puts("Network test API: interface binding, validation, busy/cancel/stale, resource cleanup, HTTP rejection and timeout passed");
}
