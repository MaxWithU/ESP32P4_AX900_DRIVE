// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ax900_radio.h"
#include <string.h>
static inline bool ax_country_valid(const ax900_radio_config_t *c){
 return c && c->country[2]==0 && (!strcmp(c->country,"CN") || !strcmp(c->country,"US") || !strcmp(c->country,"EU") || !strcmp(c->country,"JP"));
}
static inline bool ax_channel_allowed(const ax900_radio_config_t *c,unsigned channel,bool band5){
 if(!ax_country_valid(c))return false;
 if(!band5)return channel>=1 && channel<=(!strcmp(c->country,"US")?11:13);
 if(channel>=36 && channel<=48 && channel%4==0)return true;
 if(c->allow_dfs && channel>=52 && channel<=64 && channel%4==0)return true;
 return (!strcmp(c->country,"CN") || !strcmp(c->country,"US")) && channel>=149 && channel<=165 && (channel-149)%4==0;
}
static inline bool ax_frequency_allowed(const ax900_radio_config_t *c,unsigned frequency){
 if(frequency>=5000 && (frequency-5000)%5==0)return ax_channel_allowed(c,(frequency-5000)/5,true);
 return frequency>=2412 && (frequency-2407)%5==0 && ax_channel_allowed(c,(frequency-2407)/5,false);
}
