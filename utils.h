#ifndef ALLOCATOR_UTILS_H
#define ALLOCATOR_UTILS_H

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

long long calc_ptr_distance_in_bytes(void *begin, void *end);
unsigned long count_fences(void);
HEADER* last_recursive(HEADER* node);
HEADER* last(void);
unsigned long long compute_control_sum(const void *pointer, size_t size);
void header_control_sum_update(HEADER *header);
bool header_control_sum_verify(void);
bool request_more_space(int pages_to_allocate);
void update_heap_info(void);
void fill_fences(HEADER *header);
void set_header(HEADER *header, size_t mem_size, HEADER *prev, HEADER *next);
void split_headers(HEADER *header_to_reduce, size_t new_mem_size);
void connect_right(HEADER *current);
HEADER* connect_left(HEADER *current);

void *handle_memblock_null(size_t count);
void *handle_count_zero(void *memblock);
void *handle_invalid_memblock(void);

bool allocate_more_space(HEADER *handler, size_t count);

void *handle_empty_heap(size_t size);
void *handle_no_free_blocks(size_t size);
void *handle_block_of_exact_size(HEADER *current_header);
void *handle_block_of_larger_size(HEADER *current_header, size_t size);
void *handle_next_block_free(HEADER *handler, size_t count);
void *handle_next_block_free_and_far(HEADER *handler, size_t count);

#endif //ALLOCATOR_UTILS_H
