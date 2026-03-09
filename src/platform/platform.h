#pragma once

// Reserves a contiguous range of virtual address space. Does NOT use physical
// RAM.
void* os_memory_reserve(u64 size);

// Returns the native OS page size used for reserve/commit boundaries.
u64 os_memory_page_size();

// Commits physical RAM to a previously reserved range.
void os_memory_commit(void* ptr, u64 size);

// Decommits physical RAM (tells the OS you don't need it, but keeps the address
// reserved).
void os_memory_decommit(void* ptr, u64 size);

// Releases the address space back to the OS entirely.
void os_memory_release(void* ptr, u64 size);
