#ifndef REST_CONTEXT_H
#define REST_CONTEXT_H
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stddef.h>

typedef struct {
    char *json_buf;           // Buffer for JSON output
    size_t buf_size;          // Size of the buffer
    size_t *json_len;         // Pointer to store actual JSON length
    SemaphoreHandle_t sem;    // Semaphore for notification
    void *user_data;          // Optional: module-specific data or callback
} rest_json_request_t;

#endif // REST_CONTEXT_H