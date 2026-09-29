#include "ax900_channels.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 ax900_radio_config_t c={.country="CN",.allow_dfs=true};
 assert(ax_channel_allowed(&c,13,false) && ax_frequency_allowed(&c,5745));
 assert(!ax_frequency_allowed(&c,5746) && !ax_channel_allowed(&c,35,true));
 assert(ax_frequency_allowed(&c,5260));c.allow_dfs=false;assert(!ax_frequency_allowed(&c,5260));
 memcpy(c.country,"US",3);assert(!ax_channel_allowed(&c,12,false) && ax_channel_allowed(&c,165,true));
 memcpy(c.country,"EU",3);assert(ax_channel_allowed(&c,13,false) && !ax_channel_allowed(&c,149,true));
 memcpy(c.country,"JP",3);assert(!ax_channel_allowed(&c,14,false));
 memcpy(c.country,"XX",3);assert(!ax_frequency_allowed(&c,5180));
 puts("Country and DFS channel restrictions passed");
}
