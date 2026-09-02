#include "memory.h"

#include "esp_heap_caps.h"

void *memory_malloc(size_t size)
{
	return heap_caps_malloc(size, MALLOC_CAP_8BIT);
}

void *memory_calloc(size_t count, size_t size)
{
	return heap_caps_calloc(count, size, MALLOC_CAP_8BIT);
}

void *memory_realloc(void *ptr, size_t size)
{
	return heap_caps_realloc(ptr, size, MALLOC_CAP_8BIT);
}

void memory_free(void *ptr)
{
	heap_caps_free(ptr);
}

void *memory_caps_malloc(size_t size, uint32_t capabilities)
{
	return heap_caps_malloc(size, capabilities);
}

void *memory_caps_calloc(size_t count, size_t size, uint32_t capabilities)
{
	return heap_caps_calloc(count, size, capabilities);
}
