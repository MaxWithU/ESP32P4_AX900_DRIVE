// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "include/ax900_metrics.h"
void ax_metric_add(ax900_metric_t metric,int64_t delta);
void ax_metric_peak(ax900_metric_t metric,uint64_t value);
