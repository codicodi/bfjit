#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "bfjit.h"
#include "bfjit-bitops.h"
#include "bfjit-codegen.h"
#include "bfjit-memory.h"
#include "bfjit-runtime.h"

typedef struct {
    size_t jmp;
    int begin;
} loop_data;

struct bf_jit_encoder {
    unsigned char* data;
    size_t size;
    size_t cap;
    int rtc;
    int eof;
    loop_data* loops;
    size_t loops_size;
    size_t loops_cap;
    size_t copy_loop_start;
    int need_load;
    int need_store;
};

bf_jit_encoder* bf_jit_encoder_new(int rtc, int eof)
{
    bf_jit_encoder* enc = bf_realloc(NULL, sizeof(bf_jit_encoder));
    enc->data = NULL;
    enc->size = 0;
    enc->cap = 0;
    enc->rtc = rtc;
    enc->eof = eof;
    enc->loops = NULL;
    enc->loops_size = 0;
    enc->loops_cap = 0;
    return enc;
}

void bf_jit_encoder_free(bf_jit_encoder* enc)
{
    if (enc)
    {
        bf_free(enc->loops);
        bf_free(enc->data);
        bf_free(enc);
    }
}

static void enc_ensure_cap(bf_jit_encoder* enc, size_t extra_cap)
{
    if (enc->cap < enc->size + extra_cap)
    {
        enc->cap = (enc->cap == 0) ? 2048 : enc->cap * 2;
        if (enc->cap < extra_cap + enc->size)
            enc->cap = extra_cap + enc->size;
        enc->data = bf_realloc(enc->data, enc->cap);
    }
}

static void enc_write_byte(bf_jit_encoder* enc, unsigned char b)
{
    enc_ensure_cap(enc, 1);
    enc->data[enc->size++] = b;
}

static void enc_write_byte2(bf_jit_encoder* enc, unsigned char b1, unsigned char b2)
{
    enc_ensure_cap(enc, 2);
    enc->data[enc->size++] = b1;
    enc->data[enc->size++] = b2;
}

static void enc_write_byte3(bf_jit_encoder* enc, unsigned char b1, unsigned char b2,
                            unsigned char b3)
{
    enc_ensure_cap(enc, 3);
    enc->data[enc->size++] = b1;
    enc->data[enc->size++] = b2;
    enc->data[enc->size++] = b3;
}

static void enc_write_byte4(bf_jit_encoder* enc, unsigned char b1, unsigned char b2,
                            unsigned char b3, unsigned char b4)
{
    enc_ensure_cap(enc, 4);
    enc->data[enc->size++] = b1;
    enc->data[enc->size++] = b2;
    enc->data[enc->size++] = b3;
    enc->data[enc->size++] = b4;
}

static void enc_write_int(bf_jit_encoder* enc, int i)
{
    enc_ensure_cap(enc, sizeof(i));
    unsigned char* iter = (unsigned char*)&i;
    unsigned char* end = iter + sizeof(i);
    while (iter != end)
        enc->data[enc->size++] = *iter++;
}

static void enc_replace_int(bf_jit_encoder* enc, int i, size_t off)
{
    unsigned char* iter = (unsigned char*)&i;
    unsigned char* end = iter + sizeof(i);
    while (iter != end)
        enc->data[off++] = *iter++;
}

static void enc_write_ptr(bf_jit_encoder* enc, void* p)
{
    enc_ensure_cap(enc, sizeof(p));
    unsigned char* iter = (unsigned char*)&p;
    unsigned char* end = iter + sizeof(p);
    while (iter != end)
        enc->data[enc->size++] = *iter++;
}

void bf_jit_encoder_init(bf_jit_encoder* enc)
{
    // rbp       - main pointer
    // r13 & r14 - beginning and end of program memory, used for bounds checking
    // r12       - address of error function
    // r15       - address of output function
    // bl        - cached value of [rbp]

    if (enc->rtc)
    {
        enc_write_byte2(enc, 0x41, 0x54);                       // push r12
        enc_write_byte2(enc, 0x41, 0x55);                       // push r13
        enc_write_byte2(enc, 0x41, 0x56);                       // push r14
        enc_write_byte2(enc, 0x49, 0xBC);
        enc_write_ptr(enc, (void*)&bf_runtime_out_of_bounds);   // mov  r12, qword ptr out_of_bounds
#ifdef _WIN32
        enc_write_byte3(enc, 0x49, 0x89, 0xCD);                 // mov  r13, rcx
        enc_write_byte3(enc, 0x49, 0x89, 0xD6);                 // mov  r14, rdx
#else
        enc_write_byte3(enc, 0x49, 0x89, 0xFD);                 // mov  r13, rdi
        enc_write_byte3(enc, 0x49, 0x89, 0xF6);                 // mov  r14, rsi
#endif
    }
    enc_write_byte2(enc, 0x41, 0x57);                           // push r15
    enc_write_byte2(enc, 0x49, 0xBF);
    enc_write_ptr(enc, (void*)&bf_runtime_write_char);          // mov  r15, qword ptr write_char
    enc_write_byte(enc, 0x53);                                  // push rbx
    enc_write_byte2(enc, 0x31, 0xDB);                           // xor  ebx, ebx
    enc_write_byte(enc, 0x55);                                  // push rbp
#ifdef _WIN32
    enc_write_byte3(enc, 0x48, 0x89, 0xCD);                     // mov  rbp, rcx
    if (!enc->rtc)
        enc_write_byte4(enc, 0x48, 0x83, 0xEC, 0x20);           // sub  rsp, 32
    else
        enc_write_byte4(enc, 0x48, 0x83, 0xEC, 0x28);           // sub  rsp, 40
#else
    enc_write_byte3(enc, 0x48, 0x89, 0xFD);                     // mov  rbp, rdi
    if (enc->rtc)
        enc_write_byte4(enc, 0x48, 0x83, 0xEC, 0x08);           // sub  rsp, 8
#endif

    enc->need_load = 0;
    enc->need_store = 0;
}

bf_compiled_code bf_jit_encoder_finish(bf_jit_encoder* enc)
{
#ifdef _WIN32
    if (!enc->rtc)
        enc_write_byte4(enc, 0x48, 0x83, 0xC4, 0x20);           // add  rsp, 32
    else
        enc_write_byte4(enc, 0x48, 0x83, 0xC4, 0x28);           // add  rsp, 40
#else
    if (enc->rtc)
        enc_write_byte4(enc, 0x48, 0x83, 0xC4, 0x08);           // add  rsp, 8
#endif
    enc_write_byte(enc, 0x5D);                                  // pop  rbp
    enc_write_byte(enc, 0x5B);                                  // pop  rbx
    enc_write_byte2(enc, 0x41, 0x5F);                           // pop  r15
    if (enc->rtc)
    {
        enc_write_byte2(enc, 0x41, 0x5E);                       // pop  r14
        enc_write_byte2(enc, 0x41, 0x5D);                       // pop  r13
        enc_write_byte2(enc, 0x41, 0x5C);                       // pop  r12
    }
    enc_write_byte(enc, 0xC3);                                  // ret

    bf_compiled_code code;
    code.data = enc->data;
    code.size = enc->size;
    enc->data = NULL;
    enc->size = 0;
    enc->cap = 0;
    enc->loops_size = 0;
    return code;
}

void bf_jit_encode_check(bf_jit_encoder* enc, int32_t x)
{
    assert(x != 0);
    if (!enc->rtc)
        return;

    if (-128 <= x && x <= 127)
    {
        enc_write_byte4(enc, 0x48, 0x8D, 0x45, (unsigned char)x);   // lea  rax, [rbp+x]
    }
    else
    {
        enc_write_byte3(enc, 0x48, 0x8D, 0x85);
        enc_write_int(enc, x);                                      // lea  rax, [rbp+x]
    }

    if (x < 0)
    {
        enc_write_byte3(enc, 0x4C, 0x39, 0xE8);                     // cmp  rax, r13
        enc_write_byte2(enc, 0x7D, 0x03);                           // jge  3
    }
    else
    {
        enc_write_byte3(enc, 0x4C, 0x39, 0xF0);                     // cmp  rax, r14
        enc_write_byte2(enc, 0x7C, 0x03);                           // jl   3
    }
    enc_write_byte3(enc, 0x41, 0xFF, 0xD4);                         // call r12
}

static void enc_store_impl(bf_jit_encoder* enc)
{
    enc_write_byte3(enc, 0x88, 0x5D, 0x00);                                 // mov  byte ptr [rbp], bl
}

static void enc_load_impl(bf_jit_encoder* enc)
{
    enc_write_byte3(enc, 0x8A, 0x5D, 0x00);                                 // mov  bl, byte ptr [rbp]
}

static void enc_load(bf_jit_encoder* enc)
{
    if (enc->need_load)
    {
        assert(!enc->need_store);
        enc_load_impl(enc);
        enc->need_load = 0;
    }
}

static void enc_store(bf_jit_encoder* enc)
{
    if (enc->need_store)
    {
        assert(!enc->need_load);
        enc_store_impl(enc);
        enc->need_store = 0;
    }
}

static void enc_encode_next_impl(bf_jit_encoder* enc, int32_t count)
{
    if (count > 0)
    {
        if (count == 1)
        {
            enc_write_byte3(enc, 0x48, 0xFF, 0xC5);                         // inc  rbp
        }
        else if (count < 128)
        {
            enc_write_byte4(enc, 0x48, 0x83, 0xC5, (unsigned char)count);   // add  rbp, <count>
        }
        else
        {
            enc_write_byte3(enc, 0x48, 0x81, 0xC5);
            enc_write_int(enc, count);                                      // add  rbp, <count>
        }
    }
    else
    {
        if (count == -1)
        {
            enc_write_byte3(enc, 0x48, 0xFF, 0xCD);                         // dec  rbp
        }
        else if (-count < 128)
        {
            enc_write_byte4(enc, 0x48, 0x83, 0xED, (unsigned char)-count);  // sub  rbp, <-count>
        }
        else
        {
            enc_write_byte3(enc, 0x48, 0x81, 0xED);
            enc_write_int(enc, -count);                                     // sub  rbp, <-count>
        }
    }
}

void bf_jit_encode_next_unsafe(bf_jit_encoder* enc, int32_t count)
{
    assert(count != 0);
    enc_store(enc);
    enc_encode_next_impl(enc, count);
    enc->need_load = 1;
}

void bf_jit_encode_next(bf_jit_encoder* enc, int32_t count)
{
    assert(count != 0);
    bf_jit_encode_check(enc, count);
    bf_jit_encode_next_unsafe(enc, count);
}

void bf_jit_encode_add(bf_jit_encoder* enc, int32_t count)
{
    assert(count != 0);
    enc_load(enc);
    if (count == 1)
        enc_write_byte2(enc, 0xFE, 0xC3);                           // inc  bl
    else if (count == -1)
        enc_write_byte2(enc, 0xFE, 0xCB);                           // dec  bl
    else if (count > 0)
        enc_write_byte3(enc, 0x80, 0xC3, (unsigned char)count);     // add  bl, <count>
    else
        enc_write_byte3(enc, 0x80, 0xEB, (unsigned char)-count);    // sub  bl, <-count>
    enc->need_store = 1;
}

static void enc_save_loop_start(bf_jit_encoder* enc, int begin)
{
    if (enc->loops_size == enc->loops_cap)
    {
        enc->loops_cap = (enc->loops_cap == 0) ? 16 : (enc->loops_cap * 2);
        enc->loops = bf_realloc(enc->loops, enc->loops_cap * sizeof(loop_data));
    }

    loop_data l;
    l.jmp = enc->size;
    l.begin = begin;
    enc->loops[enc->loops_size++] = l;
}

void bf_jit_encode_loop_start(bf_jit_encoder* enc)
{
    enc_load(enc);
    enc_store(enc);
    // number of bytes written here must correspond to value
    // subtracted in bf_jit_pop_started_loop
    enc_write_byte2(enc, 0x84, 0xDB);                               // test bl, bl
    enc_write_byte2(enc, 0x0F, 0x84);
    enc_write_int(enc, 0);                                          // jz   <placeholder>

    enc_save_loop_start(enc, 1);
}

void bf_jit_encode_loop_start_optimized(bf_jit_encoder* enc)
{
    enc_save_loop_start(enc, 0);
}

void bf_jit_pop_started_loop(bf_jit_encoder* enc)
{
    assert(enc->loops_size != 0);
    if (enc->loops[enc->loops_size - 1].begin)
        enc->size -= 8;
    enc->loops_size -= 1;
}

size_t bf_jit_current_loop_size(bf_jit_encoder* enc)
{
    assert(enc->loops_size != 0);
    return enc->size - enc->loops[enc->loops_size - 1].jmp;
}

void bf_jit_encode_loop_end_optimized(bf_jit_encoder* enc)
{
    assert(enc->loops_size != 0);
    loop_data data = enc->loops[--enc->loops_size];
    if (data.begin)
    {
        size_t l = data.jmp;
        enc_replace_int(enc, (int)(enc->size - l), l - 4);
    }
}

void bf_jit_encode_loop_end(bf_jit_encoder* enc)
{
    assert(enc->loops_size != 0);
    loop_data data = enc->loops[--enc->loops_size];
    size_t l = data.jmp;
    enc_load(enc);
    enc_store(enc);
    enc_write_byte2(enc, 0x84, 0xDB);                                   // test bl, bl
    enc_write_byte2(enc, 0x0F, 0x85);
    enc_write_int(enc, (int)(l - 4 - enc->size));                       // jnz  <'[' location>
    if (data.begin)
        enc_replace_int(enc, (int)(enc->size - l), l - 4);
}

int bf_jit_is_in_loop(bf_jit_encoder* enc)
{
    return enc->loops_size != 0;
}

void bf_jit_encode_input(bf_jit_encoder* enc)
{
    if (enc->eof == 0)
    {
        // cell set to 0 on eof
        enc_write_byte2(enc, 0x48, 0xB8);
        enc_write_ptr(enc, (void*)&bf_runtime_read_char_eof_zero);      // mov  rax, qword ptr read_char(0)
    }
    else if (enc->eof == -1)
    {
        // cell set to -1 on eof
        enc_write_byte2(enc, 0x48, 0xB8);
        enc_write_ptr(enc, (void*)&bf_runtime_read_char_eof_minusone);  // mov  rax, qword ptr read_char(-1)
    }
    else
    {
        // cell unchanged on eof
#ifdef _WIN32
        enc_write_byte2(enc, 0x88, 0xD9);                               // mov  cl, bl
#else
        enc_write_byte3(enc, 0x40, 0x88, 0xDF);                         // mov  dil, bl
#endif
        enc_write_byte2(enc, 0x48, 0xB8);
        enc_write_ptr(enc, (void*)&bf_runtime_read_char_eof_nochange);  // mov  rax, qword ptr read_char(no change)
    }
    enc_write_byte2(enc, 0xFF, 0xD0);                                   // call rax
    enc_write_byte2(enc, 0x88, 0xC3);                                   // mov  bl, al
    enc->need_store = 1;
    enc->need_load = 0;
}

void bf_jit_encode_output(bf_jit_encoder* enc)
{
    enc_load(enc);
#ifdef _WIN32
    enc_write_byte2(enc, 0x88, 0xD9);                                   // mov  cl, bl
#else
    enc_write_byte3(enc, 0x40, 0x88, 0xDF);                             // mov  dil, bl
#endif
    enc_write_byte3(enc, 0x41, 0xFF, 0xD7);                             // call r15
}

void bf_jit_encode_offop_unsafe(bf_jit_encoder* enc, int32_t count, int32_t off)
{
    assert(count != 0 && off != 0);
    if (count > 0 && -128 <= off && off <= 127)
    {
        enc_write_byte4(enc, 0x80, 0x45,
                        (unsigned char)off,
                        (unsigned char)count);                          // add  byte ptr [rbp+<off>], <count>
    }
    else if (count > 0)
    {
        enc_write_byte2(enc, 0x80, 0x85);
        enc_write_int(enc, off);
        enc_write_byte(enc, (unsigned char)count);                      // add  byte ptr [rbp+<off>], <count>
    }
    else if (count < 0 && -128 <= off && off <= 127)
    {
        enc_write_byte4(enc, 0x80, 0x6D,
                        (unsigned char)off,
                        (unsigned char)-count);                         // sub  byte ptr [rbp+<off>], <-count>
    }
    else
    {
        enc_write_byte2(enc, 0x80, 0xAD);
        enc_write_int(enc, off);
        enc_write_byte(enc, (unsigned char)-count);                     // sub  byte ptr [rbp+<off>], <-count>
    }
}

static void enc_clear_cache(bf_jit_encoder* enc)
{
    enc_write_byte2(enc, 0x31, 0xDB);                                   // xor  ebx, ebx
}

void bf_jit_encode_set(bf_jit_encoder* enc, int32_t val)
{
    if (val == 0)
        enc_clear_cache(enc);
    else
        enc_write_byte2(enc, 0xB3, (unsigned char)val);                 // mov  bl, <val>
    enc->need_store = 1;
    enc->need_load = 0;
}

typedef size_t bf_jumpdata;

static bf_jumpdata enc_jmp_helper_forward_start(bf_jit_encoder* enc)
{
    return enc->size;
}

static void enc_jmp_helper_forward_finish(bf_jit_encoder* enc, bf_jumpdata pos)
{
    ptrdiff_t where = enc->size - pos;
    if (where > 127)
        bf_error("jump too big");
    enc->data[pos - 1] = (unsigned char)where;
}

static bf_jumpdata enc_jmp_helper_backward_start(bf_jit_encoder* enc)
{
    return enc->size;
}

static void enc_jmp_helper_backward_finish(bf_jit_encoder* enc, bf_jumpdata pos)
{
    ptrdiff_t where = pos - enc->size;
    if (where < -128)
        bf_error("jump too big");
    enc->data[enc->size - 1] = (unsigned char)where;
}

void bf_jit_start_copy_seq(bf_jit_encoder* enc)
{
    enc_load(enc);
    enc_write_byte2(enc, 0x84, 0xDB);                                   // test bl, bl
    enc_write_byte2(enc, 0x74, 0x00);                                   // jz   <end>
    enc->copy_loop_start = enc_jmp_helper_forward_start(enc);
}

void bf_jit_finish_copy_seq(bf_jit_encoder* enc)
{
    enc_jmp_helper_forward_finish(enc, enc->copy_loop_start);           // end:
}

static void enc_copyop_impl(bf_jit_encoder* enc, int32_t off, int32_t mul)
{
    if (mul > 0 && -129 < off && off < 128)
    {
        enc_write_byte3(enc, 0x00, 0x5D, (unsigned char)off);           // add  byte ptr [rbp+<off>], bl
    }
    else if (mul > 0)
    {
        enc_write_byte2(enc, 0x00, 0x9D);
        enc_write_int(enc, off);                                        // add  byte ptr [rbp+<off>], bl
    }
    else if (-129 < off && off < 128)
    {
        enc_write_byte3(enc, 0x28, 0x5D, (unsigned char)off);           // sub  byte ptr [rbp+<off>], bl
    }
    else
    {
        enc_write_byte2(enc, 0x28, 0x9D);
        enc_write_int(enc, off);                                        // sub  byte ptr [rbp+<off>], bl
    }
}

static void enc_copyop_impl_mul(bf_jit_encoder* enc, int32_t off, int32_t mul)
{
    uint32_t multiplier = mul > 0 ? mul : -mul;

    switch (multiplier)
    {
    case 2:
        enc_write_byte3(enc, 0x8D, 0x04, 0x1B);                         // lea  eax, [rbx + rbx]
        break;
    case 3:
        enc_write_byte3(enc, 0x8D, 0x04, 0x5B);                         // lea  eax, [rbx + rbx * 2]
        break;
    case 4:
        enc_write_byte3(enc, 0x8D, 0x04, 0x9D);                         // lea  eax, [rbx * 4]
        enc_write_int(enc, 0);
        break;
    case 5:
        enc_write_byte3(enc, 0x8D, 0x04, 0x9B);                         // lea  eax, [rbx + rbx * 4]
        break;
    case 6:
        enc_write_byte3(enc, 0x8D, 0x04, 0x5B);                         // lea  eax, [rbx + rbx * 2]
        enc_write_byte2(enc, 0x01, 0xC0);                               // add  eax, eax
        break;
    case 7:
        enc_write_byte3(enc, 0x8D, 0x04, 0xDD);                         // lea  eax, [rbx * 8]
        enc_write_int(enc, 0);
        enc_write_byte2(enc, 0x29, 0xD8);                               // sub  eax, ebx
        break;
    case 8:
        enc_write_byte3(enc, 0x8D, 0x04, 0xDD);                         // lea  eax, [rbx * 8]
        enc_write_int(enc, 0);
        break;
    case 9:
        enc_write_byte3(enc, 0x8D, 0x04, 0xDB);                         // lea  eax, [rbx + rbx * 8]
        break;
    case 10:
        enc_write_byte3(enc, 0x8D, 0x04, 0x9B);                         // lea  eax, [rbx + rbx * 4]
        enc_write_byte2(enc, 0x01, 0xC0);                               // add  eax, eax
        break;
    case 11:
        enc_write_byte3(enc, 0x8D, 0x04, 0x9B);                         // lea  eax, [rbx + rbx * 4]
        enc_write_byte3(enc, 0x8D, 0x04, 0x43);                         // lea  eax, [rbx + rax * 2]
        break;
    case 12:
        enc_write_byte3(enc, 0x8D, 0x04, 0x5B);                         // lea  eax, [rbx + rbx * 2]
        enc_write_byte3(enc, 0xC1, 0xE0, 0x02);                         // shl  eax, 2
        break;
    default:
        enc_write_byte2(enc, 0x88, 0xD8);                               // mov  al, bl
        if ((multiplier & (multiplier - 1)) == 0) // power of 2
        {
            unsigned index = bf_ctz(multiplier);
            enc_write_byte3(enc, 0xC0, 0xE0, (unsigned char)index);     // shl al, <index>
        }
        else
        {
            enc_write_byte(enc, 0xB9);
            enc_write_int(enc, multiplier);                             // mov  ecx, <multiplier>
            enc_write_byte2(enc, 0xF7, 0xE1);                           // mul  ecx
        }
    }

    if (mul > 0 && -129 < off && off < 128)
    {
        enc_write_byte3(enc, 0x00, 0x45, (unsigned char)off);           // add  byte ptr [rbp+<off>], al
    }
    else if (mul > 0)
    {
        enc_write_byte2(enc, 0x00, 0x85);
        enc_write_int(enc, off);                                        // add  byte ptr [rbp+<off>], al
    }
    else if (-129 < off && off < 128)
    {
        enc_write_byte3(enc, 0x28, 0x45, (unsigned char)off);           // sub  byte ptr [rbp+<off>], al
    }
    else
    {
        enc_write_byte2(enc, 0x28, 0x85);
        enc_write_int(enc, off);                                        // sub  byte ptr [rbp+<off>], al
    }
}

void bf_jit_encode_copyop_unsafe(bf_jit_encoder* enc, int32_t off, int32_t mul)
{
    assert(off != 0 && mul != 0);
    enc_load(enc);
    if (mul == 1 || mul == -1)
        enc_copyop_impl(enc, off, mul);
    else
        enc_copyop_impl_mul(enc, off, mul);
}

void bf_jit_encode_scanop(bf_jit_encoder* enc, int32_t off, int skip_init)
{
    /*  Its equivalent to this:
     *    bf_jit_encode_loop_start(enc);
     *    bf_jit_encode_next(enc, off);
     *    bf_jit_encode_loop_end(enc);
     *  but moves memory write from 'next' out of the loop.
     */
    assert(off != 0);
    bf_jumpdata j1 = 0;
    if (!skip_init)
    {
        enc_load(enc);
        enc_write_byte2(enc, 0x84, 0xDB);                                   // test bl, bl
        enc_write_byte2(enc, 0x74, 0x00);                                   // jz   <loop_end>
        j1 = enc_jmp_helper_forward_start(enc);
    }
    enc_store(enc);

    bf_jumpdata j2 = enc_jmp_helper_backward_start(enc);                    // loop_start:
    bf_jit_encode_check(enc, off);
    enc_encode_next_impl(enc, off);

    enc_write_byte4(enc, 0x80, 0x7D, 0x00, 0x00);                           // cmp  byte ptr [rbp], 0
    enc_write_byte2(enc, 0x75, 0x00);                                       // jnz  <loop_start>
    enc_jmp_helper_backward_finish(enc, j2);
    enc->need_load = 1;
    if (!skip_init)
        enc_jmp_helper_forward_finish(enc, j1);                             // loop_end:
}
