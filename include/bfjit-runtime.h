#ifndef BFJIT_RUNTIME_H
#define BFJIT_RUNTIME_H

#include <stddef.h>

#include "bfjit-codegen.h"

void bf_runtime_out_of_bounds();
unsigned char bf_runtime_read_char_eof_zero(void);
unsigned char bf_runtime_read_char_eof_minusone(void);
unsigned char bf_runtime_read_char_eof_nochange(unsigned char old);
void bf_runtime_write_char(unsigned char val);

void bf_jit_run(bf_compiled_code* code, size_t memsize);

#endif
