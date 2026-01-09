// io_fatfs.h - Minimal FATFS file access module
#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// List files in a directory (returns number of files, fills array with file names)
int io_fatfs_list_files(const char *dir_path, char file_list[][64], int max_files);

// Read entire file into buffer (returns bytes read, or -1 on error)
int io_fatfs_read_file(const char *file_path, uint8_t *buf, size_t buf_size);

// Check if file exists
bool io_fatfs_file_exists(const char *file_path);

#ifdef __cplusplus
}
#endif
