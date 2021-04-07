#ifndef BFJIT_IO_H
#define BFJIT_IO_H

#include <stddef.h>

#ifdef _WIN32
typedef void* bf_file;
#else
typedef int bf_file;
#endif

bf_file bf_open_file_read(const char* filename);
bf_file bf_open_file_write(const char* filename);
size_t bf_read_file(bf_file file, void* buff, size_t size);
void bf_write_file(bf_file file, void* data, size_t size);
void bf_close_file(bf_file file);

void bf_save_to_file(const char* filename, void* data, size_t size);

#endif
