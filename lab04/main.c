#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <dlfcn.h>

typedef struct AllocatorAPI {
    void* (*allocator_create)(void *mem, size_t size);
    void* (*my_malloc)(void *allocator, size_t size);
    void (*my_free)(void *allocator, void *ptr);
    void (*allocator_destroy)(void *allocator, size_t size);
} AllocatorAPI;

void write_message(int fd, const char *msg) {
    write(fd, msg, strlen(msg));
}

AllocatorAPI* load_allocator(const char *library_path) {
    if (library_path == NULL) {
        return NULL;
    }

    void *handle = dlopen(library_path, RTLD_LAZY);
    if (!handle) {
        write_message(STDERR_FILENO, "ERROR: Failed to load library\n");
        return NULL;
    }

    dlerror();

    AllocatorAPI *api = (AllocatorAPI *)mmap(NULL, sizeof(AllocatorAPI), PROT_READ | PROT_WRITE, 
                                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (api == MAP_FAILED) {
        write_message(STDERR_FILENO, "ERROR: mmap failed for AllocatorAPI\n");
        dlclose(handle);
        return NULL;
    }

    api->allocator_create = (void* (*)(void *, size_t))dlsym(handle, "allocator_create");
    api->my_malloc = (void* (*)(void *, size_t))dlsym(handle, "my_malloc");
    api->my_free = (void (*)(void *, void *))dlsym(handle, "my_free");
    api->allocator_destroy = (void (*)(void *, size_t))dlsym(handle, "allocator_destroy");

    char *error = dlerror();
    if (error != NULL) {
        write_message(STDERR_FILENO, "ERROR: Failed to load symbols from library\n");
        munmap(api, sizeof(AllocatorAPI));
        dlclose(handle);
        return NULL;
    }

    return api;
}

void* fallback_allocator_create(void *mem, size_t size) {
    (void)mem;
    (void)size;
    return NULL;
}

void* fallback_my_malloc(void *allocator, size_t size) {
    (void)allocator;
    return malloc(size);
}

void fallback_my_free(void *allocator, void *ptr) {
    (void)allocator;
    free(ptr);
}

void fallback_allocator_destroy(void *allocator, size_t size) {
    (void)allocator;
    (void)size;
}

AllocatorAPI* get_allocator_api(const char *library_path) {
    AllocatorAPI *api = load_allocator(library_path);
    if (api == NULL) {
        api = (AllocatorAPI *)mmap(NULL, sizeof(AllocatorAPI), PROT_READ | PROT_WRITE, 
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (api == MAP_FAILED) {
            write_message(STDERR_FILENO, "ERROR: mmap failed for fallback AllocatorAPI\n");
            return NULL;
        }
        api->allocator_create = fallback_allocator_create;
        api->my_malloc = fallback_my_malloc;
        api->my_free = fallback_my_free;
        api->allocator_destroy = fallback_allocator_destroy;
    }
    return api;
}

size_t addr_to_hex(char *buffer, size_t buffer_size, void *ptr) {
    const char *hex_chars = "0123456789abcdef";
    char temp[20];
    size_t len = 0;
    unsigned long addr = (unsigned long)ptr;

    temp[len++] = '0';
    temp[len++] = 'x';

    int started = 0;
    for(int i = sizeof(addr)*8 - 4; i >= 0; i -=4 ){
        unsigned long digit = (addr >> i) & 0xF;
        if (digit !=0 || started || i ==0) {
            temp[len++] = hex_chars[digit];
            started = 1;
        }
    }

    if (len >= buffer_size) {
        len = buffer_size -1;
    }

    memcpy(buffer, temp, len);
    buffer[len] = '\0';
    return len;
}

int test_allocator(const char *library_path) {
    AllocatorAPI *allocator_api = get_allocator_api(library_path);
    if (!allocator_api) return -1;

    size_t size = 4096;

    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED) {
        write_message(STDERR_FILENO, "ERROR: mmap failed\n");
        munmap(allocator_api, sizeof(AllocatorAPI));
        return EXIT_FAILURE;
    }

    void *allocator = NULL;
    if (allocator_api->allocator_create != fallback_allocator_create) {
        allocator = allocator_api->allocator_create(addr, size);
        if (!allocator) {
            write_message(STDERR_FILENO, "ERROR: Failed to initialize allocator\n");
            munmap(addr, size);
            munmap(allocator_api, sizeof(AllocatorAPI));
            return EXIT_FAILURE;
        }
    } else {
        allocator = NULL;
    }

    write_message(STDOUT_FILENO, "=============Allocator initialized=============\n");

    size_t alloc_size = 64;
    void *allocated_memory = allocator_api->my_malloc(allocator, alloc_size);

    if (allocated_memory == NULL) {
        write_message(STDERR_FILENO, "ERROR: Memory allocation failed\n");
    } else {
        write_message(STDOUT_FILENO, "- Memory allocated successfully\n");
    }

    if (allocated_memory != NULL) {
        write_message(STDOUT_FILENO, "- Allocated memory contains: ");

        strcpy((char*)allocated_memory, "Hello, Allocator!\n");

        write(STDOUT_FILENO, allocated_memory, strlen((char*)allocated_memory));

        char buffer[64];
        size_t len = 0;
        const char *addr_msg = "- Allocated memory address: ";
        write(STDOUT_FILENO, addr_msg, strlen(addr_msg));

        len = addr_to_hex(buffer, sizeof(buffer), allocated_memory);
        write(STDOUT_FILENO, buffer, len);
        write(STDOUT_FILENO, "\n", 1);

        allocator_api->my_free(allocator, allocated_memory);
        write_message(STDOUT_FILENO, "- Memory freed\n");
    }

    if (allocator_api->allocator_destroy != fallback_allocator_destroy) {
        allocator_api->allocator_destroy(allocator, size);
    }

    munmap(allocator_api, sizeof(AllocatorAPI));
    munmap(addr, size);

    write_message(STDOUT_FILENO, "- Allocator destroyed\n===============================================\n");
    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    const char *library_path = (argc > 1) ? argv[1] : NULL;

    if (test_allocator(library_path))  {
        return EXIT_FAILURE;
    } else {
        return EXIT_SUCCESS;
    }
}
