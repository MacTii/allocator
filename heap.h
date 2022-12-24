#ifndef ALLOCATOR_HEAP_H
#define ALLOCATOR_HEAP_H

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "custom_unistd.h"

#define FENCE_LENGTH 4
#define PAGE_SIZE 4096
#define HASH 5381
#define HEADER_STRUCT_SIZE sizeof(HEADER)
#define HEADER_SIZE(size) (HEADER_STRUCT_SIZE + (size) + 2 * FENCE_LENGTH)

#define max(a, b) ((a) > (b) ? (a) : (b))

struct header_t {
    struct header_t *ptr_prev;
    struct header_t *ptr_next;
    size_t memory_size;
    bool is_free;
    uint8_t *ptr_user_memory;
    unsigned long long c_sum;
};

typedef struct header_t HEADER;

struct heap_t {
    HEADER *head;
    size_t pages;
    size_t headers_allocated;
    unsigned long long c_sum;
};

typedef struct heap_t HEAP;

typedef enum pointer_type_t {
    pointer_null,
    pointer_heap_corrupted,
    pointer_control_block,
    pointer_inside_fences,
    pointer_inside_data_block,
    pointer_unallocated,
    pointer_valid
} pointer_type_t;

int heap_setup(void);
int heap_validate(void);
void heap_clean(void);
void *heap_malloc(size_t size);
void *heap_calloc(size_t number, size_t size);
void *heap_realloc(void *memblock, size_t count);
void heap_free(void *memblock);
size_t heap_get_largest_used_block_size(void);
enum pointer_type_t get_pointer_type(const void *pointer);

#endif
