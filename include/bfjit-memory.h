#ifndef BFJIT_MEMORY_H
#define BFJIT_MEMORY_H

#include <stddef.h>

void* bf_zero_alloc(size_t size);
void* bf_realloc(void* ptr, size_t newsize);
void bf_free(void* ptr);

void* bf_virtual_alloc(size_t size);
void bf_virtual_make_exe(void* mem, size_t size);
void bf_virtual_free(void* mem, size_t size);

#endif
