#include <stddef.h>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "bfjit.h"
#include "bfjit-io.h"

static bf_file bf_open_file_impl(const char* filename, int write)
{
#ifdef _WIN32
    bf_file f = CreateFileA(filename, (write ? GENERIC_WRITE : GENERIC_READ),
                            FILE_SHARE_READ, NULL, (write ? CREATE_ALWAYS : OPEN_EXISTING),
                            FILE_FLAG_SEQUENTIAL_SCAN, NULL);
#else
    bf_file f = open(filename, (write ? (O_WRONLY | O_CREAT | O_TRUNC) : O_RDONLY), 0666);
#endif
    if (f != (bf_file)-1)
        return f;
    bf_error("couldn't open file '%s'", filename);
}

bf_file bf_open_file_read(const char* filename)
{
    return bf_open_file_impl(filename, 0);
}

bf_file bf_open_file_write(const char* filename)
{
    return bf_open_file_impl(filename, 1);
}

void bf_close_file(bf_file file)
{
#ifdef _WIN32
    CloseHandle(file);
#else
    close(file);
#endif
}

size_t bf_read_file(bf_file file, void* buff, size_t size)
{
#ifdef _WIN32
    DWORD read;
    if (ReadFile(file, buff, (DWORD)size, &read, NULL))
        return read;
#else
    ssize_t result = read(file, buff, size);
    if (result != -1)
        return (size_t)result;
#endif
    bf_error("couldn't read from file");
}

void bf_write_file(bf_file file, void* data, size_t size)
{
#ifdef _WIN32
    DWORD written;
    if (WriteFile(file, data, (DWORD)size, &written, NULL) && written == size)
        return;
#else
    ssize_t written = write(file, data, size);
    if (!(written < 0) && (size_t)written == size)
        return;
#endif
    bf_error("couldn't write to file");
}

void bf_save_to_file(const char* filename, void* data, size_t size)
{
    bf_file f = bf_open_file_write(filename);
    bf_write_file(f, data, size);
    bf_close_file(f);
}
