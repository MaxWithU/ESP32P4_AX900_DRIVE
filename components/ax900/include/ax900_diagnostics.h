// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "sdkconfig.h"
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
#if CONFIG_AX900_FAULT_INJECTION
typedef enum { AX900_FAULT_RX=1, AX900_FAULT_LINK, AX900_FAULT_DHCP } ax900_fault_t;
esp_err_t ax900_debug_fault(ax900_fault_t fault);
#endif
#ifdef __cplusplus
}
#endif
