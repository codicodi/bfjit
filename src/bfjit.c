#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "bfjit.h"
#include "bfjit-compiler.h"
#include "bfjit-io.h"
#include "bfjit-memory.h"
#include "bfjit-runtime.h"
#include "bfjit-time.h"

static void bf_print_help(const char* argv0)
{
    printf("usage: %s <filename> [--unsafe|-u] [--debug|-d]\n"
           "  [--eof (0|-1|nochange)] [--time|-t] [--tape-size <number>] [--dump <filename>]\n",
           argv0);
}

static int bf_streq(const char* str1, const char* str2) { return strcmp(str1, str2) == 0; }

int main(int argc, char** argv)
{
    const char* source_file = NULL;
    int debug_opt = 0;
    int check_opt = 1;
    size_t tape_size = 30000;
    int eof_opt = 0;
    int dump_opt = 0;
    const char* dumpfile = NULL;
    int measure_opt = 0;

    int64_t t1 = 0, t2 = 0, t3 = 0;

    for (int i = 1; i != argc; ++i)
    {
#define next_arg() do { if (++i == argc) bf_error("no argument to '%s' option", argv[i - 1]); } while (0)

        if (bf_streq(argv[i], "--help") || bf_streq(argv[i], "-h"))
        {
            bf_print_help(argv[0]);
            return 0;
        }
        else if (bf_streq(argv[i], "--debug") || bf_streq(argv[i], "-d"))
        {
            debug_opt = 1;
        }
        else if (bf_streq(argv[i], "--unsafe") || bf_streq(argv[i], "-u"))
        {
            check_opt = 0;
        }
        else if (bf_streq(argv[i], "--tape-size"))
        {
            next_arg();
            errno = 0;
            char* end;
            long long tmp = strtoll(argv[i], &end, 10);
            if (*end != '\0' || errno != 0 || tmp <= 0)
                bf_error("invalid argument to '--tape-size' option");

            tape_size = (size_t)tmp;
        }
        else if (bf_streq(argv[i], "--eof"))
        {
            next_arg();
            if (bf_streq(argv[i], "0"))
                eof_opt = 0;
            else if (bf_streq(argv[i], "-1"))
                eof_opt = -1;
            else if (bf_streq(argv[i], "nochange"))
                eof_opt = 1;
            else
                bf_error("invalid argument to '--eof' option (possible values: '0', '-1', 'nochange')");
        }
        else if (bf_streq(argv[i], "--dump"))
        {
            next_arg();
            dump_opt = 1;
            dumpfile = argv[i];
        }
        else if (bf_streq(argv[i], "--time") || bf_streq(argv[i], "-t"))
        {
            measure_opt = 1;
        }
        else if (argv[i][0] == '-')
        {
            bf_error("unknown command line argument: '%s'", argv[i]);
        }
        else
        {
            if (source_file != NULL)
                bf_error("more than one source file specified: '%s' and '%s'", source_file, argv[i]);
            source_file = argv[i];
        }
#undef next_arg
    }

    if (source_file == NULL)
        bf_error("no source file specified");
    if (debug_opt && !check_opt)
        bf_error("'--unsafe' option is not supported in debug mode");

    if (measure_opt)
        t1 = bf_clock();

    bf_jit_encoder* enc = bf_jit_encoder_new(check_opt, eof_opt);
    bf_compiled_code code;
    if (debug_opt)
        code = bf_compile_file_debug(source_file, enc);
    else
        code = bf_compile_file(source_file, enc);

    if (measure_opt)
        t2 = bf_clock();

    if (!dump_opt)
        bf_jit_run(&code, tape_size);
    else
        bf_save_to_file(dumpfile, code.data, code.size);

    bf_jit_encoder_free(enc);
    bf_free(code.data);

    if (measure_opt)
    {
        t3 = bf_clock();

        double diff1 = (t2 - t1) / 1e6;
        double diff2 = (t3 - t2) / 1e6;
        printf("\n"
               "Compile time:   %f sec.\n"
               "Execution time: %f sec.\n"
               "Total:          %f sec.\n",
               diff1, diff2, diff1 + diff2);
    }
    return 0;
}
