#pragma once

void* pmReserveMemory(u64 size);
u64 pmGetPageSize(void);
void pmCommitMemory(void* ptr, u64 size);
void pmDecommitMemory(void* ptr, u64 size);
void pmReleaseMemory(void* ptr, u64 size);
