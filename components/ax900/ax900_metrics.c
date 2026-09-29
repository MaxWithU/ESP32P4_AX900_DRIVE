// SPDX-License-Identifier: Apache-2.0
#include "ax900_metrics.h"
#include "freertos/FreeRTOS.h"
static portMUX_TYPE metrics_lock=portMUX_INITIALIZER_UNLOCKED;
static ax900_metrics_t metrics;
void ax900_get_metrics(ax900_metrics_t *out){if(!out)return;taskENTER_CRITICAL(&metrics_lock);*out=metrics;taskEXIT_CRITICAL(&metrics_lock);}
void ax_metric_add(ax900_metric_t key,int64_t delta){
    if((unsigned)key>=AX900_METRIC_COUNT)return;
    taskENTER_CRITICAL(&metrics_lock);metrics.value[key]+=delta;taskEXIT_CRITICAL(&metrics_lock);
}
void ax_metric_peak(ax900_metric_t key,uint64_t value){
    if((unsigned)key>=AX900_METRIC_COUNT)return;
    taskENTER_CRITICAL(&metrics_lock);if(value>metrics.value[key])metrics.value[key]=value;taskEXIT_CRITICAL(&metrics_lock);
}
