#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

typedef struct Block {
    size_t size;
    int free;
    struct Block *left;
    struct Block *right;
} Block;

typedef struct Allocator {
    Block *root;
    void *memory;
    size_t total_size;
    size_t offset;
} Allocator;

int is_power_of_two(unsigned int n) {
    return (n > 0) && ((n & (n - 1)) == 0);
}

void write_error(const char *msg) {
    write(STDERR_FILENO, msg, strlen(msg));
}

size_t largest_power_of_two(size_t n) {
    size_t power = 1;
    while (power << 1 <= n) {
        power <<= 1;
    }
    return power;
}

Block *create_node(Allocator *allocator, size_t size) {
    if (allocator->offset + sizeof(Block) > allocator->total_size) {
        write_error("ERROR: Not enough memory to create a new block\n");
        return NULL;
    }

    Block *node = (Block *)((char *)allocator->memory + allocator->offset);
    allocator->offset += sizeof(Block);
    node->size = size;
    node->free = 1;
    node->left = node->right = NULL;
    return node;
}

void* allocator_create(void *mem, size_t size) {
    if (!is_power_of_two(size)) {
        write_error("ERROR: Allocator initializes memory of size not power of two\n");
        return NULL;
    }

    Allocator *allocator = (Allocator *)mem;
    allocator->memory = (char *)mem + sizeof(Allocator);
    allocator->total_size = size - sizeof(Allocator);
    allocator->offset = 0;

    size_t root_size = largest_power_of_two(allocator->total_size);
    allocator->root = create_node(allocator, root_size);
    if (!allocator->root) {
        write_error("ERROR: Unable to create root block\n");
        return NULL;
    }

    return (void *)allocator;
}

void split_node(Allocator *allocator, Block *node) {
    size_t newSize = node->size / 2;

    node->left = create_node(allocator, newSize);
    node->right = create_node(allocator, newSize);
}

Block *allocate_block(Allocator *allocator, Block *node, size_t size) {
    if (node == NULL || node->size < size || !node->free) {
        return NULL;
    }

    if (node->size == size) {
        node->free = 0;
        return node;
    }

    if (node->left == NULL) {
        split_node(allocator, node);
        if (node->left == NULL || node->right == NULL) {
            return NULL;
        }
    }

    Block *allocated = allocate_block(allocator, node->left, size);
    if (allocated == NULL) {
        allocated = allocate_block(allocator, node->right, size);
    }

    node->free = (node->left && node->left->free) && (node->right && node->right->free);
    return allocated;
}

void* my_malloc(void *allocator_ptr, size_t size) {
    if (allocator_ptr == NULL || size == 0) {
        return NULL;
    }

    Allocator *allocator = (Allocator *)allocator_ptr;

    size_t power_size = 1;
    while (power_size < size) {
        power_size <<= 1;
    }

    Block *allocated_block = allocate_block(allocator, allocator->root, power_size);
    if (allocated_block == NULL) {
        return NULL;
    }

    return (void *)((char *)allocated_block + sizeof(Block));
}

void my_free(void *allocator_ptr, void *ptr) {
    if (allocator_ptr == NULL || ptr == NULL) {
        return;
    }

    Allocator *allocator = (Allocator *)allocator_ptr;

    Block *node = (Block *)((char *)ptr - sizeof(Block));
    node->free = 1;

    while (node->left != NULL && node->left->free && node->right->free) {
        node->left = node->right = NULL;
        node->size *= 2;

        break;
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
