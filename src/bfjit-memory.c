#include <stdlib.h>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <sys/mman.h>
#endif

#include "bfjit.h"
#include "bfjit-memory.h"

#define bf_alloc_error() bf_error("couldn't allocate memory")

void* bf_zero_alloc(size_t size)
{
    void* ptr = calloc(size, 1);
    if (!ptr)
        bf_alloc_error();
    return ptr;
}

void* bf_realloc(void* ptr, size_t newsize)
{
    void* newptr;
    if ((newptr = realloc(ptr, newsize)) == NULL)
    {
        free(ptr);
        bf_alloc_error();
    }
    return newptr;
}

void bf_free(void* ptr) { free(ptr); }

void* bf_virtual_alloc(size_t size)
{
#ifdef _WIN32
    void* mem = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (mem)
        return mem;
#else
    void* mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem != MAP_FAILED)
        return mem;
#endif
    bf_alloc_error();
}

void bf_virtual_make_exe(void* mem, size_t size)
{
#ifdef _WIN32
    DWORD dummy;
    if (VirtualProtect(mem, size, PAGE_EXECUTE_READ, &dummy))
        return;
#else
    if (mprotect(mem, size, PROT_READ | PROT_EXEC) != -1)
        return;
#endif
    bf_alloc_error();
}

void bf_virtual_free(void* mem, size_t size)
{
#ifdef _WIN32
    (void)size;
    VirtualFree(mem, 0, MEM_RELEASE);
#else
    munmap(mem, size);
#endif
}
