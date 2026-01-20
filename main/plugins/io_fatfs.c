// io_fatfs.c - Minimal FATFS file access module
#include "io_fatfs.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include "io_usb_cdc_msc.h"
#include "esp_log.h"
#include <errno.h>

int io_fatfs_list_files(const char *dir_path, char file_list[][64], int max_files) {
    // if (io_usb_msc_is_enabled()) return -2; // Gated: MSC enabled (host has access)
    DIR *dir = opendir(dir_path);
    if (!dir) return -1;
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) && count < max_files) {
        if (entry->d_type == DT_REG) {
            strncpy(file_list[count], entry->d_name, 63);
            file_list[count][63] = '\0';
            count++;
        }
    }
    closedir(dir);
    return count;
}

// List directories in a path (returns number of dirs, -1 on error)
int io_fatfs_list_dirs(const char *dir_path, char dir_list[][64], int max_dirs) {
    DIR *dir = opendir(dir_path);
    if (!dir) return -1;
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) && count < max_dirs) {
        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            strncpy(dir_list[count], entry->d_name, 63);
            dir_list[count][63] = '\0';
            count++;
        }
    }
    closedir(dir);
    return count;
}

// Recursively create directories in 'dir_path'. Returns true on success (or if already exists).
bool io_fatfs_mkdir_recursive(const char *dir_path) {
    if (!dir_path || dir_path[0] == '\0') return false;
    char tmp[256];
    strncpy(tmp, dir_path, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';
    size_t len = strlen(tmp);
    if (len == 0) return false;
    if (tmp[len-1] == '/') tmp[len-1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0) {
                if (errno != EEXIST) return false;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0) {
        if (errno != EEXIST) return false;
    }
    return true;
}

int io_fatfs_read_file(const char *file_path, uint8_t *buf, size_t buf_size) {
    ESP_LOGI("io_fatfs", "Attempting to open file: %s", file_path);
    FILE *f = fopen(file_path, "rb");
    if (!f) {
        ESP_LOGE("io_fatfs", "fopen failed for %s: %s", file_path, strerror(errno));
        return -1;
    }
    int bytes_read = fread(buf, 1, buf_size, f);
    fclose(f);
    ESP_LOGI("io_fatfs", "Read %d bytes from %s", bytes_read, file_path);
    return bytes_read;
}

bool io_fatfs_file_exists(const char *file_path) {
    // if (io_usb_msc_is_enabled()) return false; // Gated: MSC enabled (host has access)
    struct stat st;
    return stat(file_path, &st) == 0;
}
