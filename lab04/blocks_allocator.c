#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

#define MAX_BLOCK_SIZE_EXTENT 10
#define MIN_BLOCK_SIZE_EXTENT 0
#define NUM_LISTS 11

typedef struct Block {
    struct Block *next;
    struct Block *prev;
    size_t size;
} Block;

typedef struct Allocator {
    Block* freeLists[NUM_LISTS];
    void *memory;
    size_t total_size;
} Allocator;

void write_error(const char *msg) {
    write(STDERR_FILENO, msg, strlen(msg));
}

size_t power_of_two(int base, int exp) {
    size_t result = 1;
    for(int i = 0; i < exp; i++) {
        result *= base;
    }
    return result;
}

int get_index(size_t size) {
    int index = 0;
    size_t block_size = 1;
    while (block_size < size && index < NUM_LISTS -1) {
        block_size <<= 1;
        index++;
    }
    return index;
}

void add_to_free_list(Allocator *allocator, Block *block) {
    int index = get_index(block->size);
    block->next = allocator->freeLists[index];
    block->prev = NULL;
    if (allocator->freeLists[index] != NULL) {
        allocator->freeLists[index]->prev = block;
    }
    allocator->freeLists[index] = block;
}

void remove_from_free_list(Allocator *allocator, Block *block) {
    int index = get_index(block->size);
    if (block->prev != NULL) {
        block->prev->next = block->next;
    } else {
        allocator->freeLists[index] = block->next;
    }
    if (block->next != NULL) {
        block->next->prev = block->prev;
    }
    block->next = block->prev = NULL;
}

void split_block(Allocator *allocator, Block* block) {
    if (block->size <= power_of_two(2, MIN_BLOCK_SIZE_EXTENT)) return;

    size_t half_size = block->size / 2;
    Block *left = (Block *)((char *)block);
    Block *right = (Block *)((char *)block + half_size);

    left->size = half_size;
    left->next = left->prev = NULL;
    right->size = half_size;
    right->next = right->prev = NULL;

    remove_from_free_list(allocator, block);

    add_to_free_list(allocator, left);
    add_to_free_list(allocator, right);
}

void* allocator_create(void* mem, size_t size) {
    Allocator* allocator = (Allocator*)mem;
    allocator->total_size = size - sizeof(Allocator);
    allocator->memory = (char*)mem + sizeof(Allocator);

    for (int i = 0; i < NUM_LISTS; ++i) {
        allocator->freeLists[i] = NULL;
    }

    size_t initial_size = power_of_two(2, MAX_BLOCK_SIZE_EXTENT);
    if (initial_size > allocator->total_size) {
        initial_size = allocator->total_size;
    }

    Block *initial_block = (Block *)allocator->memory;
    initial_block->size = initial_size;
    initial_block->next = NULL;
    initial_block->prev = NULL;

    add_to_free_list(allocator, initial_block);

    return (void *)allocator;
}

void* my_malloc(void *allocator_ptr, size_t size) {
    if (allocator_ptr == NULL || size == 0) {
        return NULL;
    }

    Allocator *allocator = (Allocator *)allocator_ptr;

    size_t actual_size = 1;
    while (actual_size < size) {
        actual_size <<= 1;
    }

    int index = get_index(actual_size);
    if (index >= NUM_LISTS) {
        write_error("ERROR: Requested size exceeds maximum block size\n");
        return NULL;
    }

    int current_index = index;
    while (current_index < NUM_LISTS && allocator->freeLists[current_index] == NULL) {
        current_index++;
    }

    if (current_index == NUM_LISTS) {
        write_error("ERROR: No available blocks for allocation\n");
        return NULL;
    }

    while (current_index > index) {
        Block *block_to_split = allocator->freeLists[current_index];
        if (block_to_split == NULL) {
            write_error("ERROR: No block available to split\n");
            return NULL;
        }
        split_block(allocator, block_to_split);
        current_index--;
    }

    Block *allocated_block = allocator->freeLists[index];
    if (allocated_block == NULL) {
        write_error("ERROR: Failed to allocate block\n");
        return NULL;
    }
    remove_from_free_list(allocator, allocated_block);

    return (void *)allocated_block;
}

void my_free(void *allocator_ptr, void *ptr) {
    if (allocator_ptr == NULL || ptr == NULL) {
        return;
    }

    Allocator *allocator = (Allocator *)allocator_ptr;
    Block *block = (Block*)ptr;
    add_to_free_list(allocator, block);

    while (block->size < power_of_two(2, MAX_BLOCK_SIZE_EXTENT)) {
        size_t size = block->size;
        size_t offset = (char*)block - (char*)allocator->memory;
        size_t buddy_offset = offset ^ size;
        Block *buddy = (Block*)((char*)allocator->memory + buddy_offset);

        int index = get_index(size);
        Block *current = allocator->freeLists[index];
        int found = 0;
        while (current != NULL) {
            if (current == buddy) {
                found = 1;
                break;
            }
            current = current->next;
        }

        if (!found) {
            break;
        }

        remove_from_free_list(allocator, buddy);

        Block *merged_block = (buddy < block) ? buddy : block;
        merged_block->size = size * 2;

        remove_from_free_list(allocator, block);

        block = merged_block;
        add_to_free_list(allocator, merged_block);
    }
}

void allocator_destroy(void *allocator_ptr, size_t size) {
    if (!allocator_ptr) {
        return;
    }

    Allocator *allocator = (Allocator *)allocator_ptr;
    void *full_memory = (char *)allocator->memory - sizeof(Allocator);

    if (munmap(full_memory, size) == -1) {
        const char msg[] = "ERROR: munmap failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        _exit(EXIT_FAILURE);
    }
}
