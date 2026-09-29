#pragma once
#include <stdio.h>
#define ESP_LOGW(tag, format, ...) fprintf(stderr, tag ": " format "\n", ##__VA_ARGS__)
