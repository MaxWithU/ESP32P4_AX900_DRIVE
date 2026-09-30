// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ax900_help.h"
// Worker/TLS diagnostics. A generic failure must not overwrite a specific cause.
void ax_issue_set(ax900_issue_t issue);
void ax_issue_if_clear(ax900_issue_t issue);
