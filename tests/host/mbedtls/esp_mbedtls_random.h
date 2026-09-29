#pragma once
#include <stdlib.h>
static inline int mbedtls_esp_random(void *ctx, unsigned char *out, size_t len)
{ (void)ctx; arc4random_buf(out,len); return 0; }
