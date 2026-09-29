// SPDX-License-Identifier: Apache-2.0
#include "ax900_pool.h"
#include "ax900_metrics.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 assert(ax_pool_init()==ESP_OK);ax_packet_t *tx[24],*rx[32],*auth[8];
 for(unsigned i=0;i<24;i++){tx[i]=ax_pool_acquire(AX_PACKET_TX);assert(tx[i]);}
 assert(!ax_pool_acquire(AX_PACKET_TX));
 for(unsigned i=0;i<32;i++){rx[i]=ax_pool_acquire(AX_PACKET_RX);assert(rx[i]);}
 assert(!ax_pool_acquire(AX_PACKET_RX));
 // Authentication retains its own buffers even under full application load.
 for(unsigned i=0;i<8;i++){auth[i]=ax_pool_acquire(AX_PACKET_EAPOL);assert(auth[i]);}
 assert(!ax_pool_acquire(AX_PACKET_EAPOL));assert(ax_pool_used()==64);
 assert(ax_pool_destroy()==ESP_ERR_INVALID_STATE);
 for(unsigned i=0;i<24;i++)ax_pool_release(tx[i]);
 for(unsigned i=0;i<32;i++)ax_pool_release(rx[i]);
 for(unsigned i=0;i<8;i++)ax_pool_release(auth[i]);
 assert(!ax_pool_used());ax900_metrics_t m;ax900_get_metrics(&m);
 assert(m.value[AX900_POOL_PEAK]==64 && !m.value[AX900_POOL_USED]);
 assert(ax_pool_destroy()==ESP_OK);assert(ax_pool_init()==ESP_OK);assert(ax_pool_destroy()==ESP_OK);
 puts("Packet pool reservation, exhaustion, outstanding reference and reuse passed");
}
