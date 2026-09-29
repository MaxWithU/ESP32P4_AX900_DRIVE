// SPDX-License-Identifier: Apache-2.0
// Regression for smooth_ui_toolkit v2.0.0 retargeting before its first frame.
#include <cassert>
#include <cmath>
#include <cstring>
#include <new>
#include "animation/generators/spring/spring.h"

int main() {
    using smooth_ui_toolkit::Spring;
    alignas(Spring) unsigned char storage[sizeof(Spring)];
    // Force uninitialized floats to NaN instead of relying on allocator contents.
    std::memset(storage, 0xff, sizeof(storage));
    auto *spring = new (storage) Spring;
    spring->start = spring->end = 785;
    spring->springOptions.visualDuration = 0.6f;
    spring->springOptions.bounce = 0.2f;
    spring->init();
    spring->retarget(785, 309);
    assert(std::isfinite(spring->springOptions.velocity));
    assert(spring->next(5.0f));
    assert(std::isfinite(spring->value));
    assert(std::abs(spring->value - 309) < 0.1f);
    spring->~Spring();
}
