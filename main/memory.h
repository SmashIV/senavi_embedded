#ifndef SENAVI_MEMORY_H
#define SENAVI_MEMORY_H

#include <stddef.h>
#include <stdint.h>

void *memory_malloc(size_t size);
void *memory_calloc(size_t count, size_t size);
void *memory_realloc(void *ptr, size_t size);
void memory_free(void *ptr);
void *memory_caps_malloc(size_t size, uint32_t capabilities);
void *memory_caps_calloc(size_t count, size_t size, uint32_t capabilities);

#endif
