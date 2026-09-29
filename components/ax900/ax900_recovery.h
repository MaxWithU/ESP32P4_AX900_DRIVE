// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>

// Caller serializes access. A successful but flapping link retains its budget.
#define AX_RETRY_LIMIT 5
#define AX_STABLE_US INT64_C(60000000)
typedef struct {
    bool enabled, pending, online;
    unsigned attempts;
    uint32_t intent;
    int64_t due, stable_since;
} ax_recovery_t;
static inline void ax_recovery_select(ax_recovery_t *r, bool enabled) {
    uint32_t intent=r->intent+1;
    *r=(ax_recovery_t){.enabled=enabled,.intent=intent};
}
static inline void ax_recovery_lost(ax_recovery_t *r, int64_t now) {
    r->online=false;
    if(!r->enabled || r->pending)return;
    if(r->attempts>=AX_RETRY_LIMIT){r->enabled=false;return;}
    r->pending=true;r->due=now+(INT64_C(1000000)<<r->attempts);
}
static inline bool ax_recovery_take(ax_recovery_t *r, int64_t now) {
    if(!r->enabled || !r->pending || now<r->due)return false;
    r->pending=false;r->attempts++;return true;
}
static inline void ax_recovery_online(ax_recovery_t *r, int64_t now) {
    if(!r->online){r->online=true;r->stable_since=now;r->pending=false;}
    if(now-r->stable_since>=AX_STABLE_US)r->attempts=0;
}
