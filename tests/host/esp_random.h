#pragma once
#include <stdlib.h>
static inline void esp_fill_random(void *p,size_t n){arc4random_buf(p,n);}
