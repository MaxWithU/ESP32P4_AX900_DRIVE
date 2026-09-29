#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "esp_err.h"
#ifndef SO_BINDTODEVICE
#define SO_BINDTODEVICE 25
#endif
#define mem_calloc calloc
typedef struct {uint32_t addr;} ip4_addr_t;
typedef struct {uint32_t addr[4];} ip6_addr_t;
typedef struct {int type;union {ip4_addr_t ip4;ip6_addr_t ip6;} u_addr;} ip_addr_t;
#define IPADDR_TYPE_V4 0
#define IPADDR_TYPE_V6 6
static const ip_addr_t test_any={0};
#define IP_ADDR_ANY (&test_any)
#define ip_2_ip4(p) (&(p)->u_addr.ip4)
#define ip_2_ip6(p) (&(p)->u_addr.ip6)
#define IP_IS_V4(p) ((p)->type==0)
#define IP_IS_V6(p) ((p)->type==6)
#define IP_SET_TYPE_VAL(p,t) ((p).type=(t))
#define ip_addr_copy(dst,src) ((dst)=(src))
#define inet_addr_to_ip4addr(dst,src) ((dst)->addr=(src)->s_addr)
#define inet_addr_from_ip4addr(dst,src) ((dst)->s_addr=(src)->addr)
#define inet6_addr_to_ip6addr(dst,src) memcpy((dst),(src),16)
#define inet6_addr_from_ip6addr(dst,src) memcpy((dst),(src),16)
static inline int ip6_addr_isipv4mappedipv6(const ip6_addr_t *p){(void)p;return 0;}
struct __attribute__((packed)) ip_hdr {uint8_t version_ihl,tos;uint16_t length,id,fragment;uint8_t ttl,protocol;uint16_t checksum;uint32_t source,destination;};
struct __attribute__((packed)) icmp_echo_hdr {uint8_t type,code;uint16_t chksum,id,seqno;};
struct __attribute__((packed)) ip6_hdr {uint32_t version;uint16_t payload;uint8_t next,hop;uint8_t addresses[32];};
struct __attribute__((packed)) icmp6_echo_hdr {uint8_t type,code;uint16_t checksum,id,seqno;};
#define IPH_V(p) ((p)->version_ihl>>4)
#define IPH_HL_BYTES(p) (((p)->version_ihl&15)*4)
#define IPH_TTL(p) ((p)->ttl)
#define IPH_TOS(p) ((p)->tos)
#define IPH_LEN(p) ((p)->length)
#define IPH_PROTO(p) ((p)->protocol)
#define IP6H_PLEN(p) ((p)->payload)
#define lwip_ntohs ntohs
#define IP_PROTO_ICMP 1
#define IP6_NEXTH_ICMP6 58
#define ICMP_ECHO 8
#define ICMP_ER 0
#define ICMP6_TYPE_EREQ 128
#define ICMP6_TYPE_EREP 129
static inline uint16_t inet_chksum(const void *p,unsigned n){(void)p;(void)n;return 0;}
