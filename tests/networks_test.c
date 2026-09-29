// SPDX-License-Identifier: Apache-2.0
#include "ax900_networks.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    ax900_ap_t aps[8] = {0};
    for (unsigned i=0;i<8;i++) {
        memcpy(aps[i].raw_ssid,"Lab",3);aps[i].ssid_len=3;
        aps[i].frequency=2412;aps[i].rssi=-30;aps[i].bssid[5]=i;
        aps[i].secured=true;aps[i].wpa2_psk=true;
    }
    aps[1].frequency=5180;aps[1].rssi=-65;
    aps[2].frequency=5745;aps[2].rssi=-45;
    aps[3].enterprise=true;aps[3].wpa2_psk=false;
    aps[4].secured=false;aps[4].wpa2_psk=false;
    aps[5].pmf_required=true;
    aps[6].ssid_len=aps[7].ssid_len=0;
    ax900_network_t out[8];
    size_t n=ax900_group_networks(aps,8,out,8);
    assert(n==6 && out[0].access_points==3);
    assert(out[0].preferred.bssid[5]==2 && out[0].band_24 && out[0].band_5);
    // A sanitized display name is never a network identity (including embedded NUL).
    aps[1]=aps[0];aps[1].raw_ssid[1]=0;strcpy(aps[0].ssid,"L?b");strcpy(aps[1].ssid,"L?b");
    assert(ax900_group_networks(aps,2,out,8)==2);
    assert(ax900_group_networks(aps,8,out,0)==0);
    assert(ax900_group_networks(aps,8,out,1)==1);
    aps[1]=aps[0];aps[1].sae=true;
    assert(ax900_group_networks(aps,2,out,8)==2);
    puts("Network grouping tests passed");
}
