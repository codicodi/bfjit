#ifndef BFJIT_COMPILER_H
#define BFJIT_COMPILER_H

#include "bfjit-codegen.h"

bf_compiled_code bf_compile_file(const char* filename, bf_jit_encoder* enc);
bf_compiled_code bf_compile_file_debug(const char* filename, bf_jit_encoder* enc);

#endif
