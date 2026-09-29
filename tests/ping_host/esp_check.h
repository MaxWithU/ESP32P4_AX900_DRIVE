#pragma once
#include "esp_err.h"
#define ESP_GOTO_ON_FALSE(a,e,label,...) do { if (!(a)) { ret=(e); goto label; } } while (0)
