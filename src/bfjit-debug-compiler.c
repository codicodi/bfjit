#include <string.h>

#include "bfjit.h"
#include "bfjit-codegen.h"
#include "bfjit-io.h"

enum { BF_PATTERN_ADD, BF_PATTERN_SUB, BF_PATTERN_NEXT, BF_PATTERN_PREV, BF_PATTERN_NONE };

typedef struct {
    int pattern;
    int32_t patterncount;
} bf_generator_debug;

static void bf_flush_pattern(bf_jit_encoder* enc, bf_generator_debug* gen, int newpattern)
{
    if (newpattern == gen->pattern)
        return;

    if (gen->pattern == BF_PATTERN_ADD)
        bf_jit_encode_add(enc, gen->patterncount);
    else if (gen->pattern == BF_PATTERN_SUB)
        bf_jit_encode_add(enc, -gen->patterncount);
    else if (gen->pattern == BF_PATTERN_NEXT)
        bf_jit_encode_next(enc, gen->patterncount);
    else if (gen->pattern == BF_PATTERN_PREV)
        bf_jit_encode_next(enc, -gen->patterncount);

    gen->pattern = newpattern;
    gen->patterncount = 0;
}

bf_compiled_code bf_compile_file_debug(const char* filename, bf_jit_encoder* enc)
{
    bf_generator_debug gen = {BF_PATTERN_NONE, 0};
    bf_jit_encoder_init(enc);

    bf_file file = bf_open_file_read(filename);
    char input_buffer[8 * 1024];
    size_t input_size;

    do {
        input_size = bf_read_file(file, input_buffer, sizeof(input_buffer));

        for (const char* input = input_buffer; input != input_buffer + input_size; ++input)
        {
            switch (*input)
            {
            case '-':
            {
                bf_flush_pattern(enc, &gen, BF_PATTERN_SUB);
                gen.patterncount += 1;
                break;
            }
            case '+':
            {
                bf_flush_pattern(enc, &gen, BF_PATTERN_ADD);
                gen.patterncount += 1;
                break;
            }
            case '<':
            {
                bf_flush_pattern(enc, &gen, BF_PATTERN_PREV);
                gen.patterncount += 1;
                break;
            }
            case '>':
            {
                bf_flush_pattern(enc, &gen, BF_PATTERN_NEXT);
                gen.patterncount += 1;
                break;
            }
            case '.':
            {
                bf_flush_pattern(enc, &gen, BF_PATTERN_NONE);
                bf_jit_encode_output(enc);
                break;
            }
            case ',':
            {
                bf_flush_pattern(enc, &gen, BF_PATTERN_NONE);
                bf_jit_encode_input(enc);
                break;
            }
            case '[':
            {
                bf_flush_pattern(enc, &gen, BF_PATTERN_NONE);
                bf_jit_encode_loop_start(enc);
                break;
            }
            case ']':
            {
                if (!bf_jit_is_in_loop(enc))
                    bf_error("']' without a matching '['");

                bf_flush_pattern(enc, &gen, BF_PATTERN_NONE);
                bf_jit_encode_loop_end(enc);
                break;
            }
            }
        }
    } while (input_size == sizeof(input_buffer));

    if (bf_jit_is_in_loop(enc))
        bf_error("'[' without a matching ']'");

    bf_close_file(file);
    bf_flush_pattern(enc, &gen, BF_PATTERN_NONE);
    return bf_jit_encoder_finish(enc);
}
