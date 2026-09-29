// SPDX-License-Identifier: Apache-2.0
#pragma once
// Private copy of the ESP-IDF ping API with capability-aware task allocation.
#ifdef ESP_PLATFORM
#define esp_ping_new_session ax900_ping_new_session
#define esp_ping_delete_session ax900_ping_delete_session
#define esp_ping_start ax900_ping_start
#define esp_ping_stop ax900_ping_stop
#define esp_ping_get_profile ax900_ping_get_profile
#endif
#include "ping/ping_sock.h"
