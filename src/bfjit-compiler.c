#include <string.h>

#include "bfjit.h"
#include "bfjit-codegen.h"
#include "bfjit-compiler.h"
#include "bfjit-io.h"
#include "bfjit-memory.h"

typedef struct {
    int32_t off;
    int32_t op;
} bf_off_op;

typedef struct {
    int pending_set;
    int value_known;
    unsigned char current_value;
    int loop_counter_known;
    unsigned char loop_counter;
    int32_t offset;
    int32_t inplace_op;
    bf_off_op* offset_ops;
    size_t offset_ops_size;
    size_t offset_ops_cap;
} bf_generator;

static void bf_generator_init(bf_generator* gen)
{
    gen->pending_set = 0;
    gen->value_known = 1;
    gen->current_value = 0;
    gen->loop_counter_known = 0;
    gen->offset = 0;
    gen->inplace_op = 0;
    gen->offset_ops = NULL;
    gen->offset_ops_size = 0;
    gen->offset_ops_cap = 0;
}

static void bf_generator_clear_trivial_data(bf_generator* gen)
{
    gen->pending_set = 0;
    gen->offset = 0;
    gen->inplace_op = 0;
    gen->offset_ops_size = 0;
}

static void bf_generator_add_op(bf_generator* gen, int32_t op)
{
    if (gen->offset == 0)
    {
        gen->inplace_op += op;
        return;
    }

    for (size_t i = 0; i != gen->offset_ops_size; ++i)
    {
        if (gen->offset_ops[i].off == gen->offset)
        {
            gen->offset_ops[i].op += op;
            return;
        }
    }

    if (gen->offset_ops_size == gen->offset_ops_cap)
    {
        size_t newcap = (gen->offset_ops_cap == 0) ? 16 : (gen->offset_ops_cap * 2);
        gen->offset_ops = bf_realloc(gen->offset_ops, newcap * sizeof(bf_off_op));
        gen->offset_ops_cap = newcap;
    }
    bf_off_op newop;
    newop.off = gen->offset;
    newop.op = op;
    gen->offset_ops[gen->offset_ops_size++] = newop;
}

static void bf_flush_trivial_ops(bf_jit_encoder* enc, bf_generator* gen)
{
    int32_t delayed_op = 0;

    int32_t min = 0;
    int32_t max = 0;
    for (uint32_t i = 0; i != gen->offset_ops_size; ++i)
    {
        int32_t off = gen->offset_ops[i].off;
        if (off < min)
            min = off;
        if (off > max)
            max = off;
    }

    if (gen->offset < min)
        min = gen->offset;
    if (gen->offset > max)
        max = gen->offset;

    if (min != 0)
        bf_jit_encode_check(enc, min);
    if (max != 0)
        bf_jit_encode_check(enc, max);

    for (size_t i = 0; i != gen->offset_ops_size; ++i)
    {
        int32_t off = gen->offset_ops[i].off;
        int32_t op = gen->offset_ops[i].op;

        if (off == gen->offset)
        {
            delayed_op = op;
            continue;
        }
        if (op != 0)
            bf_jit_encode_offop_unsafe(enc, op, off);
    }

    if (gen->pending_set || (gen->inplace_op && gen->value_known == 1))
    {
        gen->current_value = (unsigned char)(gen->current_value + gen->inplace_op);
        bf_jit_encode_set(enc, gen->current_value);
    }
    else if (gen->inplace_op)
    {
        gen->value_known = 0;
        bf_jit_encode_add(enc, gen->inplace_op);
    }

    if (gen->offset)
    {
        gen->value_known = 0;
        bf_jit_encode_next_unsafe(enc, gen->offset);
    }
    if (delayed_op)
        bf_jit_encode_add(enc, delayed_op);

    bf_generator_clear_trivial_data(gen);
}

static int bf_optimize_trivial_loop(bf_generator* gen, bf_jit_encoder* enc)
{
    if (gen->inplace_op == 0 && gen->offset == 0)
    {
        // infinite loop
        // lets make it loop infinitely even faster by removing side effects
        gen->offset_ops_size = 0;
        return 0;
    }
    else if (gen->offset_ops_size == 0 && gen->inplace_op == 0)
    {
        bf_jit_pop_started_loop(enc);
        bf_jit_encode_scanop(enc, gen->offset, gen->loop_counter_known != 0);
        return 1;
    }
    else if (gen->offset == 0 && gen->offset_ops_size == 0 &&
             (gen->inplace_op == -1 || gen->inplace_op == 1))
    {
        bf_jit_pop_started_loop(enc);
        return 2;
    }
    else if (gen->offset == 0 && gen->inplace_op == -1)
    {
        bf_jit_pop_started_loop(enc);

        if (gen->loop_counter_known == 0)
            bf_jit_start_copy_seq(enc);

        int32_t min = 0;
        int32_t max = 0;
        for (uint32_t i = 0; i != gen->offset_ops_size; ++i)
        {
            int32_t off = gen->offset_ops[i].off;
            if (off < min)
                min = off;
            if (off > max)
                max = off;
        }

        if (min != 0)
            bf_jit_encode_check(enc, min);
        if (max != 0)
            bf_jit_encode_check(enc, max);

        for (uint32_t i = 0; i != gen->offset_ops_size; ++i)
        {
            int32_t off = gen->offset_ops[i].off;
            int32_t op = gen->offset_ops[i].op;
            if (gen->loop_counter_known == 1 && op != 0)
                bf_jit_encode_offop_unsafe(enc, op * gen->loop_counter, off);
            else if (op != 0)
                bf_jit_encode_copyop_unsafe(enc, off, op);
        }
        if (gen->loop_counter_known == 0)
            bf_jit_finish_copy_seq(enc);
        return 2;
    }
    return 0;
}

bf_compiled_code bf_compile_file(const char* filename, bf_jit_encoder* enc)
{
    bf_generator gen;
    bf_generator_init(&gen);
    bf_jit_encoder_init(enc);

    bf_file file = bf_open_file_read(filename);
    char input_buffer[8 * 1024];
    size_t input_size;

    int skipping_loop = 0;
    unsigned unmatched = 0;

    do
    {
        input_size = bf_read_file(file, input_buffer, sizeof(input_buffer));

        for (const char* input = input_buffer; input != input_buffer + input_size; ++input)
        {
            if (skipping_loop)
            {
                if (*input == ']' && unmatched == 1)
                    unmatched = 0, skipping_loop = 0;
                else if (*input == ']')
                    --unmatched;
                else if (*input == '[')
                    ++unmatched;
                continue;
            }

            switch (*input)
            {
            case '-':
            {
                bf_generator_add_op(&gen, -1);
                break;
            }
            case '+':
            {
                bf_generator_add_op(&gen, 1);
                break;
            }
            case '<':
            {
                gen.offset -= 1;
                break;
            }
            case '>':
            {
                gen.offset += 1;
                break;
            }
            case '.':
            {
                bf_flush_trivial_ops(enc, &gen);
                bf_jit_encode_output(enc);
                break;
            }
            case ',':
            {
                bf_flush_trivial_ops(enc, &gen);
                bf_jit_encode_input(enc);
                gen.value_known = 0;
                break;
            }
            case '[':
            {
                // if we know to be at zero, loop never gets executed - skip until matching ']'
                if (gen.value_known == 1 && gen.current_value == 0 && gen.offset == 0 &&
                    gen.inplace_op == 0)
                {
                    skipping_loop = 1;
                    unmatched = 1;
                    break;
                }

                bf_flush_trivial_ops(enc, &gen);

                if ((gen.value_known == 1 && gen.current_value != 0) || gen.value_known == -1)
                {
                    if (gen.value_known == 1)
                    {
                        gen.loop_counter_known = 1;
                        gen.loop_counter = gen.current_value;
                    }
                    else
                    {
                        gen.loop_counter_known = -1;
                    }
                    bf_jit_encode_loop_start_optimized(enc);
                }
                else
                {
                    gen.loop_counter_known = 0;
                    bf_jit_encode_loop_start(enc);
                }
                gen.value_known = -1;
                break;
            }
            case ']':
            {
                if (!bf_jit_is_in_loop(enc))
                    bf_error("']' without a matching '['");

                int loop_optimized = 0;
                if (bf_jit_current_loop_size(enc) == 0 && !gen.pending_set)
                    loop_optimized = bf_optimize_trivial_loop(&gen, enc);

                if (loop_optimized)
                {
                    bf_generator_clear_trivial_data(&gen);
                    if (loop_optimized == 2)
                    {
                        gen.pending_set = 1;
                    }
                }
                else
                {
                    int delay_zero_set = gen.value_known == 1 && gen.current_value == 0 &&
                                         gen.pending_set && gen.offset == 0 && gen.inplace_op == 0;

                    if (delay_zero_set)
                        gen.pending_set = 0;

                    bf_flush_trivial_ops(enc, &gen);

                    if (delay_zero_set)
                        gen.pending_set = 1;

                    if (gen.value_known == 1 && gen.current_value == 0)
                        bf_jit_encode_loop_end_optimized(enc);
                    else
                        bf_jit_encode_loop_end(enc);
                }
                gen.value_known = 1;
                gen.current_value = 0;
                gen.loop_counter_known = 0;
                break;
            }
            }
        }
    } while (input_size == sizeof(input_buffer));

    if (unmatched != 0 || bf_jit_is_in_loop(enc))
        bf_error("'[' without a matching ']'");

    bf_close_file(file);
    bf_free(gen.offset_ops);
    return bf_jit_encoder_finish(enc);
}
