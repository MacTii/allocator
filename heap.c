#include <stddef.h>
#include "heap.h"
#include "tested_declarations.h"
#include "utils.h"

static HEAP *heap = NULL;

long long calc_ptr_distance_in_bytes(void *begin, void *end) {
    if (begin == NULL || end == NULL)
        return 0;
    return (intptr_t) end - (intptr_t) begin;
}

unsigned long count_fences(void) {
    if (heap->head == NULL)
        return 0;

    unsigned int sum = 0;
    for (HEADER *current_header = heap->head; current_header; current_header = current_header->ptr_next) {
        for (int i = 0; i < FENCE_LENGTH; i++) {
            if (*((uint8_t *) current_header + i + HEADER_STRUCT_SIZE) == 'f') sum++;
            if (*((uint8_t *) current_header->ptr_user_memory + i + current_header->memory_size) == 'F') sum++;
        }
    }
    return sum;
}

HEADER *last_recursive(HEADER *node) {
    if (!node) {
        return NULL;
    }

    if (!node->ptr_next) {
        return node;
    }

    return last_recursive(node->ptr_next);
}

HEADER *last(void) {
    return last_recursive(heap->head);
}

unsigned long long compute_control_sum(const void *pointer, size_t size) {
    unsigned long long hash = HASH;
    const uint8_t *ptr = (const uint8_t *) pointer;
    for (size_t i = 0; i < size; i++) {
        hash = ((hash << 5) + hash) + ptr[i];
    }
    return hash;
}

void header_control_sum_update(HEADER *header) {
    header->c_sum = compute_control_sum(header, offsetof(HEADER, c_sum));
}

bool header_control_sum_verify(void) {
    for (HEADER *iterator = heap->head; iterator; iterator = iterator->ptr_next) {
        unsigned long long control_sum = compute_control_sum(iterator, offsetof(HEADER, c_sum));
        if (control_sum != iterator->c_sum)
            return false;
    }
    return true;
}

bool request_more_space(int pages_to_allocate) {
    uint8_t *handler = custom_sbrk(PAGE_SIZE * pages_to_allocate);
    if (handler == (void*)(-1))
        return false;
    heap->pages += pages_to_allocate;
    return true;
}

void update_heap_info(void) {
    heap->headers_allocated++;
    heap->c_sum += 2 * FENCE_LENGTH;
}

void fill_fences(HEADER *header) {
    memset((uint8_t *) header + HEADER_STRUCT_SIZE, 'f', FENCE_LENGTH);
    memset((uint8_t *) header->ptr_user_memory + header->memory_size, 'F', FENCE_LENGTH);
    header_control_sum_update(header);
}

void set_header(HEADER *header, size_t mem_size, HEADER *prev, HEADER *next) {
    header->is_free = false;
    header->memory_size = mem_size;
    header->ptr_prev = prev;
    header->ptr_next = next;
    header->ptr_user_memory = (uint8_t *) header + HEADER_STRUCT_SIZE + FENCE_LENGTH;
    if (next != NULL) {
        next->ptr_prev = header, header_control_sum_update(next);
    }
    if (prev != NULL) {
        prev->ptr_next = header;
        header_control_sum_update(prev);
    }
    fill_fences(header);
    update_heap_info();
}

void split_headers(HEADER *header_to_reduce, size_t new_mem_size) {
    size_t remaining_size = header_to_reduce->memory_size - HEADER_SIZE(new_mem_size);

    header_to_reduce->memory_size = new_mem_size;
    header_to_reduce->is_free = false;
    fill_fences(header_to_reduce);

    HEADER *new_header = (HEADER *) ((uint8_t *) header_to_reduce->ptr_user_memory + new_mem_size + FENCE_LENGTH);
    set_header(new_header, remaining_size, header_to_reduce, header_to_reduce->ptr_next);
    new_header->is_free = true;

    header_to_reduce->ptr_next = new_header;
    header_control_sum_update(new_header);
    header_control_sum_update(header_to_reduce);
}

void connect_right(HEADER *current) {
    if (current == NULL) {
        return;
    }

    HEADER *next = current->ptr_next;
    if (next == NULL) {
        return;
    }

    current->memory_size += HEADER_SIZE(next->memory_size);
    current->ptr_next = next->ptr_next;

    if (next->ptr_next != NULL) {
        next->ptr_next->ptr_prev = current;
        header_control_sum_update(next->ptr_next);
    }

    header_control_sum_update(current);
    heap->c_sum -= FENCE_LENGTH * 2;
    heap->headers_allocated--;
}

HEADER *connect_left(HEADER *current) {
    if (current == NULL) {
        return NULL;
    }

    HEADER *prev = current->ptr_prev;
    if (prev == NULL) {
        return current;
    }

    prev->memory_size += HEADER_SIZE(current->memory_size);
    prev->ptr_next = current->ptr_next;
    if (current->ptr_next != NULL) {
        current->ptr_next->ptr_prev = prev;
        header_control_sum_update(current->ptr_next);
    }
    header_control_sum_update(current);
    heap->c_sum -= FENCE_LENGTH * 2;
    heap->headers_allocated--;
    return prev;
}

void *handle_memblock_null(size_t count) {
    return heap_malloc(count);
}

void *handle_count_zero(void *memblock) {
    heap_free(memblock);
    return NULL;
}

void *handle_invalid_memblock(void) {
    return NULL;
}

bool allocate_more_space(HEADER *handler, size_t count) {
    long long left_mem = calc_ptr_distance_in_bytes((uint8_t *) handler->ptr_user_memory + handler->memory_size,
                                                    (uint8_t *) heap + heap->pages * PAGE_SIZE - FENCE_LENGTH);

    if (left_mem >= (long long) count) {
        handler->memory_size = count;
        fill_fences(handler);
        return true;
    }

    int pages_to_allocate = max((int) ((count - left_mem + PAGE_SIZE - 1) / PAGE_SIZE), 1);

    if (!request_more_space(pages_to_allocate)) {
        return false;
    }

    handler->memory_size = count;
    return fill_fences(handler), true;
}

void *handle_empty_heap(size_t size) {
    if (heap->pages * PAGE_SIZE - sizeof(HEAP) < HEADER_SIZE(size)) {
        int pages_to_allocate =
                (int) ((HEADER_SIZE(size) - (PAGE_SIZE * heap->pages - sizeof(HEAP))) / PAGE_SIZE) + 1;
        return request_more_space(pages_to_allocate) == false ? NULL : heap_malloc(size);
    }
    heap->head = (HEADER *) ((uint8_t *) heap + sizeof(HEAP));
    set_header(heap->head, size, NULL, NULL);
    return heap->head->ptr_user_memory;
}

void *handle_no_free_blocks(size_t size) {
    HEADER *last_header = last();

    long long free_mem_size = calc_ptr_distance_in_bytes(
            (uint8_t *) last_header->ptr_user_memory + last_header->memory_size + FENCE_LENGTH,
            (uint8_t *) heap + heap->pages * PAGE_SIZE) - PAGE_SIZE;

    if (free_mem_size <= (long long) (HEADER_SIZE(size))) {
        int pages_to_allocate = (int) ((HEADER_SIZE(size) - free_mem_size) / PAGE_SIZE + 1);
        pages_to_allocate = pages_to_allocate == 0 ? 1 : pages_to_allocate;
        return request_more_space(pages_to_allocate) == false ? NULL : heap_malloc(size);
    }

    HEADER *new_header = (HEADER *) ((uint8_t *) last_header->ptr_user_memory + last_header->memory_size + FENCE_LENGTH);
    set_header(new_header, size, last_header, NULL);
    return last()->ptr_user_memory;
}

void *handle_block_of_exact_size(HEADER *current_header) {
    current_header->is_free = false;
    return header_control_sum_update(current_header), current_header->ptr_user_memory;
}

void *handle_block_of_larger_size(HEADER *current_header, size_t size) {
    return split_headers(current_header, size), current_header->ptr_user_memory;
}

void *handle_next_block_free(HEADER *handler, size_t count) {
    HEADER *reduced = (HEADER *) ((uint8_t *) handler->ptr_user_memory + count + FENCE_LENGTH);
    long long reduced_size = (long long) (handler->memory_size + handler->ptr_next->memory_size - count);
    HEADER copy;

    memcpy(&copy, handler->ptr_next, sizeof(HEADER));
    reduced->ptr_next = copy.ptr_next;

    if (copy.ptr_next) {
        copy.ptr_next->ptr_prev = reduced;
        header_control_sum_update(copy.ptr_next);
    }
    reduced->is_free = true;
    reduced->ptr_prev = handler;
    reduced->memory_size = reduced_size;
    reduced->ptr_user_memory = (uint8_t *) reduced + FENCE_LENGTH + HEADER_STRUCT_SIZE;
    fill_fences(reduced);

    handler->ptr_next = reduced;
    handler->memory_size = count;
    fill_fences(handler);
    return handler->ptr_user_memory;
}

void *handle_next_block_free_and_far(HEADER *handler, size_t count) {
    if (handler->ptr_next->ptr_next) {
        handler->ptr_next->ptr_next->ptr_prev = handler;
        header_control_sum_update(handler->ptr_next->ptr_next);
    }

    handler->ptr_next = handler->ptr_next->ptr_next;
    handler->memory_size = count;

    fill_fences(handler);

    heap->c_sum -= 2 * FENCE_LENGTH;
    heap->headers_allocated--;

    return handler->ptr_user_memory;
}


int heap_setup(void) {
    heap = (HEAP *) custom_sbrk(PAGE_SIZE);
    if (heap == (void*)(-1)) {
        return -1;
    }

    heap->headers_allocated = 0;
    heap->pages = 1;
    heap->c_sum = 0;
    heap->head = NULL;
    return 0;
}

int heap_validate(void) {
    return (heap == NULL) ? 2 : (!header_control_sum_verify()) ? 3 : (heap->c_sum != count_fences()) ? 1 : 0;
}


void heap_clean(void) {
    if (2 == heap_validate()) {
        return;
    }

    unsigned long mem_size = heap->pages * PAGE_SIZE;
    memset(heap, 0, mem_size);

    heap->head = NULL;
    heap = NULL;

    custom_sbrk(-mem_size);
}

void *heap_malloc(size_t size) {
    if (size < 1 || heap_validate() || HEADER_SIZE(size) < size)
        return NULL;

    if (!heap->head) {
        return handle_empty_heap(size);
    }

    for (HEADER *current_header = heap->head; current_header; current_header = current_header->ptr_next) {
        if (current_header->is_free && current_header->memory_size == size) {
            return handle_block_of_exact_size(current_header);
        } else if (current_header->is_free &&
                   current_header->memory_size > HEADER_SIZE(size) + 1) {
            return handle_block_of_larger_size(current_header, size);
        } else if (current_header->is_free && current_header->memory_size > size) {
            current_header->memory_size = size;
            current_header->is_free = false;
            return fill_fences(current_header), current_header->ptr_user_memory;
        }
    }

    return handle_no_free_blocks(size);
}


void *heap_calloc(size_t number, size_t size) {
    void *ptr = heap_malloc(number * size);
    if (ptr != NULL) {
        memset(ptr, 0, number * size);
        return ptr;
    } else {
        return NULL;
    }
}

void *heap_realloc(void *memblock, size_t count) {
    if (heap_validate()) {
        return NULL;
    }

    if (memblock == NULL) {
        return handle_memblock_null(count);
    }

    if (count == 0) {
        return handle_count_zero(memblock);
    }

    if (get_pointer_type(memblock) != pointer_valid) {
        return handle_invalid_memblock();
    }

    HEADER *header = (HEADER *) ((uint8_t *) memblock - FENCE_LENGTH - HEADER_STRUCT_SIZE);

    if (count == header->memory_size) {
        return header_control_sum_update(header), header->ptr_user_memory;
    } else if (count < header->memory_size) {
        return header->memory_size = count, fill_fences(header), header->ptr_user_memory;
    }

    if (header->ptr_next == NULL) {
        if (allocate_more_space(header, count)) {
            return header->ptr_user_memory;
        } else {
            return NULL;
        }
    } else if (header->ptr_next->is_free && header->memory_size + header->ptr_next->memory_size > count) {
        return handle_next_block_free(header, count);
    } else if (header->ptr_next->is_free && calc_ptr_distance_in_bytes(header->ptr_user_memory,
                                                                       (uint8_t *) header->ptr_next->ptr_user_memory +
                                                                       header->ptr_next->memory_size) > (long long) count) {
        return handle_next_block_free_and_far(header, count);
    }

    void *ptr = heap_malloc(count);
    if (ptr == NULL) {
        return NULL;
    }

    memcpy(ptr, header->ptr_user_memory, header->memory_size);
    heap_free(header->ptr_user_memory);
    header_control_sum_update((HEADER *) ((uint8_t *) ptr - HEADER_STRUCT_SIZE - FENCE_LENGTH));

    return ptr;
}

void heap_free(void *memblock) {
    if (2 == heap_validate() || !memblock || get_pointer_type(memblock) != pointer_valid) return;

    HEADER *header = (HEADER *) ((uint8_t *) memblock - FENCE_LENGTH - HEADER_STRUCT_SIZE);
    header->is_free = true;

    HEADER *next = header->ptr_next;
    HEADER *prev = header->ptr_prev;

    if (prev != NULL && prev->is_free) {
        header = connect_left(header);
    }
    if (next != NULL && next->is_free) {
        connect_right(header);
    }
    if (header->ptr_next != NULL) {
        header->memory_size = calc_ptr_distance_in_bytes(header, header->ptr_next) - HEADER_SIZE(0);
    }
    fill_fences(header);
}

size_t heap_get_largest_used_block_size(void) {
    if (heap == NULL || heap->head == NULL || heap_validate() != 0) {
        return 0;
    }

    size_t max_size = 0;
    for (HEADER *current_header = heap->head; current_header != NULL; current_header = current_header->ptr_next) {
        if (!current_header->is_free) {
            max_size = max(max_size, current_header->memory_size);
        }
    }
    return max_size;
}

enum pointer_type_t get_pointer_type(const void *const pointer) {
    if (!pointer) return pointer_null;
    if (heap_validate() == 1) return pointer_heap_corrupted;
    if ((intptr_t) pointer < (intptr_t) heap) return pointer_unallocated;
    if ((intptr_t) pointer < (intptr_t) ((uint8_t *) heap + sizeof(HEAP))) return pointer_control_block;
    if (heap->head == NULL) return pointer_unallocated;

    HEADER *current_header = heap->head;
    for (; current_header->ptr_next &&
           (intptr_t) current_header->ptr_next <= (intptr_t) pointer; current_header = current_header->ptr_next);

    intptr_t user_mem = (intptr_t) ((uint8_t *) current_header->ptr_user_memory + current_header->memory_size);
    intptr_t left_fences = (intptr_t) ((uint8_t *) current_header + FENCE_LENGTH + HEADER_STRUCT_SIZE);
    intptr_t right_fences = (intptr_t) ((uint8_t *) current_header->ptr_user_memory + current_header->memory_size +
                                        FENCE_LENGTH);
    intptr_t control_block = (intptr_t) ((uint8_t *) current_header + HEADER_STRUCT_SIZE);

    if ((intptr_t) pointer < control_block) {
        return pointer_control_block;
    } else if ((intptr_t) pointer < left_fences && !current_header->is_free) {
        return pointer_inside_fences;
    } else if ((intptr_t) pointer == (intptr_t) current_header->ptr_user_memory && !current_header->is_free) {
        return pointer_valid;
    } else if ((intptr_t) pointer == (intptr_t) current_header->ptr_user_memory) {
        return pointer_unallocated;
    } else if ((intptr_t) pointer < user_mem && !current_header->is_free) {
        return pointer_inside_data_block;
    } else if ((intptr_t) pointer < user_mem) {
        return pointer_unallocated;
    } else if ((intptr_t) pointer < right_fences && !current_header->is_free) {
        return pointer_inside_fences;
    }
    return pointer_unallocated;
}
