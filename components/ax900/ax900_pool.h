// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define AX_PACKET_COUNT 64
typedef enum { AX_PACKET_TX, AX_PACKET_RX, AX_PACKET_EAPOL } ax_packet_kind_t;
typedef struct ax_packet {uint32_t generation;size_t len;bool encrypted;uint8_t bytes[1518];} ax_packet_t;
esp_err_t ax_pool_init(void);
ax_packet_t *ax_pool_acquire(ax_packet_kind_t kind);
void ax_pool_release(ax_packet_t *packet);
// Refuse to free storage while lwIP still holds a zero-copy RX reference.
esp_err_t ax_pool_destroy(void);
unsigned ax_pool_used(void);
