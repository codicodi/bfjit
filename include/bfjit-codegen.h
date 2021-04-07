#ifndef BFJIT_CODEGEN_H
#define BFJIT_CODEGEN_H

#include <stdint.h>

typedef struct {
    unsigned char* data;
    size_t size;
} bf_compiled_code;

typedef struct bf_jit_encoder bf_jit_encoder;

bf_jit_encoder* bf_jit_encoder_new(int rtc, int eof);
void bf_jit_encoder_free(bf_jit_encoder*);

void bf_jit_encoder_init(bf_jit_encoder* enc);
bf_compiled_code bf_jit_encoder_finish(bf_jit_encoder* enc);

void bf_jit_encode_check(bf_jit_encoder* enc, int32_t count);
void bf_jit_encode_next_unsafe(bf_jit_encoder* enc, int32_t count);
void bf_jit_encode_next(bf_jit_encoder* enc, int32_t count);
void bf_jit_encode_add(bf_jit_encoder* enc, int32_t count);
void bf_jit_encode_input(bf_jit_encoder* enc);
void bf_jit_encode_output(bf_jit_encoder* enc);
void bf_jit_encode_offop_unsafe(bf_jit_encoder* enc, int32_t count, int32_t off);
void bf_jit_encode_set(bf_jit_encoder* enc, int32_t val);
void bf_jit_encode_scanop(bf_jit_encoder* enc, int32_t off, int skip_init);

void bf_jit_start_copy_seq(bf_jit_encoder* enc);
void bf_jit_encode_copyop_unsafe(bf_jit_encoder* enc, int32_t off, int32_t mul);
void bf_jit_finish_copy_seq(bf_jit_encoder* enc);

void bf_jit_encode_loop_start(bf_jit_encoder* enc);
void bf_jit_encode_loop_start_optimized(bf_jit_encoder* enc);
void bf_jit_encode_loop_end_optimized(bf_jit_encoder* enc);
void bf_jit_encode_loop_end(bf_jit_encoder* enc);
void bf_jit_pop_started_loop(bf_jit_encoder* enc);
size_t bf_jit_current_loop_size(bf_jit_encoder* enc);
int bf_jit_is_in_loop(bf_jit_encoder* enc);

#endif
