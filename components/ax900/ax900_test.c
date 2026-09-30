// SPDX-License-Identifier: Apache-2.0
#include "ax900_test.h"
#include "ax900.h"
#include "ax900_dns.h"
#include "ax900_netif_dns.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include <stdatomic.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static portMUX_TYPE test_lock=portMUX_INITIALIZER_UNLOCKED;
static ax900_test_result_t published;
static atomic_bool cancelled;
struct test {
    ax900_test_config_t config;
    ax900_test_result_t result;
    int index;
    int64_t start,deadline;
    uint32_t source,dns;
};
static uint32_t millis(void){return (uint32_t)(esp_timer_get_time()/1000);}
static bool current(struct test *t){
    if(!ax900_connection_is_current(t->result.connection_id)){t->result.state=AX900_TEST_STALE;return false;}
    if(atomic_load(&cancelled)){t->result.state=AX900_TEST_CANCELLED;return false;}
    if(esp_timer_get_time()>=t->deadline){t->result.error=ESP_ERR_TIMEOUT;return false;}
    return true;
}
static int wait_socket(struct test *t,int fd,bool writing,int64_t until){
    while(current(t)){
        int64_t left=until-esp_timer_get_time();if(left<=0){errno=ETIMEDOUT;return 0;}
        if(left>100000)left=100000;
        struct timeval tv={.tv_sec=0,.tv_usec=left};fd_set fds;FD_ZERO(&fds);FD_SET(fd,&fds);
        int n=select(fd+1,writing?NULL:&fds,writing?&fds:NULL,NULL,&tv);
        if(n>0)return 1;if(n<0 && errno!=EINTR)return -1;
    }return -1;
}
static int open_socket(struct test *t,int type){
    int fd=socket(AF_INET,type,0);if(fd<0)return -1;
    struct ifreq iface={0};struct sockaddr_in local={.sin_family=AF_INET,.sin_addr.s_addr=t->source};
    if(!if_indextoname(t->index,iface.ifr_name) ||
       setsockopt(fd,SOL_SOCKET,SO_BINDTODEVICE,&iface,sizeof(iface)) ||
       bind(fd,(struct sockaddr *)&local,sizeof(local)) || fcntl(fd,F_SETFL,O_NONBLOCK)<0){close(fd);return -1;}
    return fd;
}
static bool send_all(struct test *t,int fd,const void *data,size_t n){
    const uint8_t *p=data;
    while(n && current(t)){
        int sent=send(fd,p,n,0);
        if(sent>0){p+=sent;n-=sent;continue;}
        if(sent<0 && (errno==EAGAIN || errno==EWOULDBLOCK)){if(wait_socket(t,fd,true,t->deadline)>0)continue;}
        return false;
    }return n==0;
}
static bool receive_all(struct test *t,int fd,void *data,size_t n){
    uint8_t *p=data;
    while(n && current(t)){
        int got=recv(fd,p,n,0);
        if(got>0){p+=got;n-=got;continue;}
        if(got<0 && (errno==EAGAIN || errno==EWOULDBLOCK)){if(wait_socket(t,fd,false,t->deadline)>0)continue;}
        return false;
    }return n==0;
}
static bool http_line(struct test *t,int fd,char *line,size_t capacity,size_t *budget){
    size_t used=0;
    while(used<capacity-1 && *budget){
        if(!receive_all(t,fd,line+used,1))return false;
        --*budget;
        if(line[used++]=='\n'){
            line[used]=0;
            return used>=2 && line[used-2]=='\r';
        }
    }
    t->result.error=ESP_ERR_INVALID_RESPONSE;return false;
}
static bool http_response(struct test *t,int fd){
    char line[512];size_t budget=8192;
    t->result.stage=AX900_TEST_STAGE_RESPONSE;
    // Informational responses have their own header section. Only a final
    // response establishes success; cap bytes and count as well as elapsed time.
    for(unsigned interim=0;interim<=8;interim++){
        if(!http_line(t,fd,line,sizeof(line),&budget))return false;
        size_t n=strlen(line);
        if(n<14 || memcmp(line,"HTTP/1.",7) || (line[7]!='0' && line[7]!='1') ||
           line[8]!=' ' || line[9]<'1' || line[9]>'5' || line[10]<'0' || line[10]>'9' ||
           line[11]<'0' || line[11]>'9' || (line[12]!=' ' && line[12]!='\r'))break;
        unsigned status=(line[9]-'0')*100+(line[10]-'0')*10+line[11]-'0';
        if(status>=200){t->result.http_status=status;return status<400;}
        // No protocol upgrade was requested, so 101 cannot complete this GET.
        if(status==101)break;
        do{if(!http_line(t,fd,line,sizeof(line),&budget))return false;}while(strcmp(line,"\r\n"));
    }
    t->result.error=ESP_ERR_INVALID_RESPONSE;return false;
}
static bool resolve(struct test *t,uint32_t *address){
    if(t->config.kind!=AX900_TEST_DNS && inet_pton(AF_INET,t->config.host,address)==1)return true;
    t->result.stage=AX900_TEST_STAGE_DNS;
    uint8_t q[272],reply[512];uint16_t id=esp_random();size_t n=ax_dns_question(q,sizeof(q),id,t->config.host);
    if(!n || !t->dns){t->result.error=ESP_ERR_INVALID_ARG;return false;}
    int fd=open_socket(t,SOCK_DGRAM);if(fd<0)return false;
    struct sockaddr_in server={.sin_family=AF_INET,.sin_port=htons(53),.sin_addr.s_addr=t->dns};
    bool ok=false;
    if(connect(fd,(struct sockaddr *)&server,sizeof(server))==0 && send(fd,q,n,0)==(int)n){
        while(wait_socket(t,fd,false,t->deadline)>0){
            int got=recv(fd,reply,sizeof(reply),0);
            if(got>0 && ax_dns_answer(reply,got,q,n,(uint8_t *)address)){ok=true;break;}
        }
    }
    close(fd);return ok;
}
static bool run_test(struct test *t){
    uint32_t address;if(!resolve(t,&address))return false;
    if(t->config.kind==AX900_TEST_DNS)return true;
    t->result.stage=AX900_TEST_STAGE_SOCKET;
    int fd=open_socket(t,t->config.kind==AX900_TEST_UDP?SOCK_DGRAM:SOCK_STREAM);if(fd<0)return false;
    struct sockaddr_in peer={.sin_family=AF_INET,.sin_port=htons(t->config.port),.sin_addr.s_addr=address};
    t->result.stage=AX900_TEST_STAGE_CONNECT;bool ok=false;
    int rc=connect(fd,(struct sockaddr *)&peer,sizeof(peer));
    if(rc && errno==EINPROGRESS){
        if(wait_socket(t,fd,true,t->deadline)<=0)goto done;
        int err=0;socklen_t n=sizeof(err);if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&err,&n) || err){errno=err;goto done;}
    }else if(rc)goto done;
    if(t->config.kind==AX900_TEST_TCP){ok=true;goto done;}
    t->result.stage=AX900_TEST_STAGE_TRANSFER;
    if(t->config.kind==AX900_TEST_HTTP){
        char request[512];int n=snprintf(request,sizeof(request),"GET %s HTTP/1.1\r\nHost: %s:%u\r\nConnection: close\r\n\r\n",t->config.path,t->config.host,t->config.port);
        if(n<0 || n>=sizeof(request) || !send_all(t,fd,request,n))goto done;
        ok=http_response(t,fd);
    }else if(t->config.kind==AX900_TEST_UDP){
        uint8_t packet[1200],reply[1200];memset(packet,0xa5,sizeof(packet));memcpy(packet,"AX9P",4);
        uint32_t nonce=esp_random();memcpy(packet+4,&nonce,4);uint64_t sum=0;
        // Round-trip UDP delivery at a bounded rate; never label this one-way loss.
        for(uint32_t seq=0;seq<100 && current(t);seq++){
            uint32_t wire=htonl(seq);memcpy(packet+8,&wire,4);uint32_t begin=millis();
            if(send(fd,packet,sizeof(packet),0)!=(int)sizeof(packet))goto done;
            t->result.sent++;int64_t until=esp_timer_get_time()+200000;
            while(wait_socket(t,fd,false,until)>0){
                int got=recv(fd,reply,sizeof(reply),0);
                if(got==sizeof(packet) && !memcmp(packet,reply,sizeof(packet))){
                    uint32_t delay=millis()-begin;sum+=delay;t->result.received++;t->result.bytes+=got;
                    if(t->result.received==1 || delay<t->result.min_ms)t->result.min_ms=delay;
                    if(delay>t->result.max_ms)t->result.max_ms=delay;break;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if(t->result.received)t->result.average_ms=sum/t->result.received;
        ok=t->result.sent==100 && t->result.received==100;
    }else{
        uint8_t block[4096];memset(block,0xa5,sizeof(block));
        uint32_t header[3];memcpy(header,t->config.kind==AX900_TEST_UPLOAD?"AX9U":"AX9D",4);
        header[1]=htonl(t->config.bytes);header[2]=esp_random();
        if(!send_all(t,fd,header,sizeof(header)))goto done;
        uint32_t begin=millis();
        while(t->result.bytes<t->config.bytes && current(t)){
            uint32_t left=t->config.bytes-t->result.bytes;size_t n=left<sizeof(block)?left:sizeof(block);
            if(t->config.kind==AX900_TEST_UPLOAD){if(!send_all(t,fd,block,n))goto done;}
            else {if(!receive_all(t,fd,block,n))goto done;for(size_t i=0;i<n;i++)if(block[i]!=0xa5){t->result.error=ESP_ERR_INVALID_RESPONSE;goto done;}}
            t->result.bytes+=n;
        }
        t->result.stage=AX900_TEST_STAGE_RESPONSE;
        if(t->config.kind==AX900_TEST_UPLOAD){
            uint32_t acknowledged=0;if(!receive_all(t,fd,&acknowledged,4))goto done;
            if(ntohl(acknowledged)!=t->config.bytes){t->result.error=ESP_ERR_INVALID_RESPONSE;goto done;}
        }
        uint32_t elapsed=millis()-begin;
        t->result.kilobits_per_second=elapsed?(uint64_t)t->result.bytes*8/elapsed:0;
        ok=t->result.bytes==t->config.bytes;
    }
 done:
    if(!ok)t->result.socket_error=errno;
    close(fd);return ok && current(t);
}
static void test_task(void *arg){
    struct test *t=arg;
    t->start=esp_timer_get_time();t->deadline=t->start+(int64_t)t->config.timeout_ms*1000;
    t->result.internal_before=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    errno=0;bool ok=run_test(t);
    if(!ok && !t->result.error)t->result.error=ESP_FAIL;
    if(t->result.state==AX900_TEST_RUNNING)t->result.state=ok?AX900_TEST_PASSED:AX900_TEST_FAILED;
    t->result.elapsed_ms=(esp_timer_get_time()-t->start)/1000;
    t->result.internal_after=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    t->result.internal_minimum=heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    taskENTER_CRITICAL(&test_lock);published=t->result;taskEXIT_CRITICAL(&test_lock);
    free(t);
#if CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM && CONFIG_SPIRAM
    vTaskDeleteWithCaps(NULL);
#else
    vTaskDelete(NULL);
#endif
}
static esp_err_t start_failed(esp_err_t error){
    taskENTER_CRITICAL(&test_lock);published.state=AX900_TEST_FAILED;published.error=error;taskEXIT_CRITICAL(&test_lock);
    return error;
}
esp_err_t ax900_test_start(const ax900_test_config_t *config){
    if(!config || config->kind>AX900_TEST_UDP || config->kind<0 ||
       !memchr(config->host,0,sizeof(config->host)) || !config->host[0] ||
       !memchr(config->dns_ipv4,0,sizeof(config->dns_ipv4)) || !memchr(config->path,0,sizeof(config->path)) ||
       config->timeout_ms<1000 || config->timeout_ms>60000 || !config->port)return ESP_ERR_INVALID_ARG;
    for(const char *p=config->host;*p;p++)if((unsigned char)*p<=32 || *p=='/' || *p=='@' || *p==127)return ESP_ERR_INVALID_ARG;
    if(config->kind==AX900_TEST_HTTP){
        if(config->path[0]!='/')return ESP_ERR_INVALID_ARG;
        for(const char *p=config->path;*p;p++)if((unsigned char)*p<=32 || *p==127)return ESP_ERR_INVALID_ARG;
    }
    if((config->kind==AX900_TEST_UPLOAD || config->kind==AX900_TEST_DOWNLOAD) && (config->bytes<1024 || config->bytes>8388608))return ESP_ERR_INVALID_ARG;
    // Reserve before looking up the interface. Stop invalidates public link state
    // first, then waits for RUNNING tests before destroying the netif.
    taskENTER_CRITICAL(&test_lock);
    if(published.state==AX900_TEST_RUNNING){taskEXIT_CRITICAL(&test_lock);return ESP_ERR_INVALID_STATE;}
    published=(ax900_test_result_t){.state=AX900_TEST_RUNNING,.kind=config->kind,.stage=AX900_TEST_STAGE_INTERFACE};
    atomic_store(&cancelled,false);taskEXIT_CRITICAL(&test_lock);
    ax900_link_status_t link;ax900_get_link_status(&link);
    if(!link.has_ip || !link.authenticated)return start_failed(ESP_ERR_INVALID_STATE);
    esp_netif_t *netif=esp_netif_get_handle_from_ifkey("AX900");esp_netif_ip_info_t ip;
    if(!netif || esp_netif_get_ip_info(netif,&ip)!=ESP_OK)return start_failed(ESP_ERR_INVALID_STATE);
    struct test *t=calloc(1,sizeof(*t));if(!t)return start_failed(ESP_ERR_NO_MEM);
    t->config=*config;t->index=esp_netif_get_netif_impl_index(netif);t->source=ip.ip.addr;
    if(config->dns_ipv4[0]){if(inet_pton(AF_INET,config->dns_ipv4,&t->dns)!=1){free(t);return start_failed(ESP_ERR_INVALID_ARG);}}
    else{
        esp_netif_dns_info_t dns={0};esp_err_t e=ax_netif_get_dns(netif,&dns);
        if(e==ESP_OK && dns.ip.type==ESP_IPADDR_TYPE_V4)t->dns=dns.ip.u_addr.ip4.addr;
        uint32_t literal;
        if((config->kind==AX900_TEST_DNS || inet_pton(AF_INET,config->host,&literal)!=1) && !t->dns){
            taskENTER_CRITICAL(&test_lock);published.stage=AX900_TEST_STAGE_DNS;taskEXIT_CRITICAL(&test_lock);
            free(t);return start_failed(e==ESP_OK?ESP_ERR_NOT_FOUND:e);
        }
    }
    t->result=(ax900_test_result_t){.state=AX900_TEST_RUNNING,.kind=config->kind,.connection_id=link.connection_id};
    if(t->index<=0 || !t->source || !ax900_connection_is_current(link.connection_id)){free(t);return start_failed(ESP_ERR_INVALID_STATE);}
    taskENTER_CRITICAL(&test_lock);published=t->result;taskEXIT_CRITICAL(&test_lock);
#if CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM && CONFIG_SPIRAM
    BaseType_t made=xTaskCreateWithCaps(test_task,"ax900-test",8192,t,2,NULL,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
    BaseType_t made=xTaskCreate(test_task,"ax900-test",8192,t,2,NULL);
#endif
    if(made!=pdPASS){free(t);return start_failed(ESP_ERR_NO_MEM);}
    return ESP_OK;
}
void ax900_test_cancel(void){atomic_store(&cancelled,true);}
void ax900_test_get_result(ax900_test_result_t *out){
    if(!out)return;taskENTER_CRITICAL(&test_lock);*out=published;taskEXIT_CRITICAL(&test_lock);
    if(out->state!=AX900_TEST_IDLE && out->state!=AX900_TEST_RUNNING && !ax900_connection_is_current(out->connection_id))out->state=AX900_TEST_STALE;
}
