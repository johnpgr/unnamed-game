#pragma once

#include "base/base_core.h"

void* reserve_system_memory(u64 size);
u64 get_system_page_size(void);
void commit_system_memory(void* ptr, u64 size);
void decommit_system_memory(void* ptr, u64 size);
void release_system_memory(void* ptr, u64 size);
