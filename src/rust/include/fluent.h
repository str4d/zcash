// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef FLUENT_INCLUDE_H_
#define FLUENT_INCLUDE_H_

#include "rust/types.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct FluentContext;
typedef struct FluentContext FluentContext;

/// Initializes the Fluent translation component.
FluentContext* fluent_init(
    const codeunit* log_path,
    size_t log_path_len,
    const char* initial_filter,
    bool log_timestamps);

/// Frees a tracing handle returned from `fluent_init`;
void fluent_free(FluentContext* handle);

#ifdef __cplusplus
}
#endif

#endif // FLUENT_INCLUDE_H_
