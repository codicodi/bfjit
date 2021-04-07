#include <string.h>

#include "bfjit.h"
#include "bfjit-memory.h"
#include "bfjit-runtime.h"

void bf_runtime_out_of_bounds() { bf_error("out of bounds memory access"); }

#ifdef _WIN32
#define bf_getchar _getchar_nolock
#define bf_putchar _putchar_nolock
#else
#define bf_getchar getchar_unlocked
#define bf_putchar putchar_unlocked
#endif

unsigned char bf_runtime_read_char_eof_zero()
{
    int c = bf_getchar();
    return c == EOF ? 0 : (unsigned char)c;
}

unsigned char bf_runtime_read_char_eof_minusone()
{
    int c = bf_getchar();
    return c == EOF ? (unsigned char)-1 : (unsigned char)c;
}

unsigned char bf_runtime_read_char_eof_nochange(unsigned char old)
{
    int c = bf_getchar();
    return c == EOF ? old : (unsigned char)c;
}

void bf_runtime_write_char(unsigned char val) { bf_putchar(val); }

void bf_jit_run(bf_compiled_code* code, size_t tapesize)
{
    size_t memsize = code->size;
    void* mem = bf_virtual_alloc(memsize);

    memcpy(mem, code->data, memsize);
    bf_virtual_make_exe(mem, memsize);

    typedef void (*compiled_func_type)(unsigned char*, unsigned char*);
    compiled_func_type compiled_func = (compiled_func_type)mem;

    unsigned char* program_memory = bf_zero_alloc(tapesize);
    compiled_func(program_memory, program_memory + tapesize);

    bf_free(program_memory);
    bf_virtual_free(mem, memsize);
}
