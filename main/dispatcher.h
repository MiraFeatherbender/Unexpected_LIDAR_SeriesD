#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define BUF_SIZE (1024)

// enums and structs for communication state and context

typedef enum {
#define X_MODULE(name) TARGET##name,
#include "modules.def"
#undef X_MODULE
    TARGET_MAX
} dispatch_target_t;

static const char * const target_names[] = {
#define X_MODULE(name) "TARGET" #name,
#include "modules.def"
#undef X_MODULE
};

typedef enum {
#define X_MODULE(name) SOURCE##name,
#include "modules.def"
#undef X_MODULE
    SOURCE_UNDEFINED
} dispatch_source_t;

static const char * const source_names[] = {
#define X_MODULE(name) "SOURCE" #name,
#include "modules.def"
#undef X_MODULE
};


typedef struct {
    dispatch_source_t source;
    dispatch_target_t targets[TARGET_MAX];
    size_t message_len;
    uint8_t data[BUF_SIZE];
    void *context; // optional context pointer
} dispatcher_msg_t;

typedef struct pool_msg_s pool_msg_t;

// Helper: fill target array with sentinel
static inline void dispatcher_fill_targets_impl(dispatch_target_t *targets, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        targets[i] = TARGET_MAX;
    }
}

#define dispatcher_fill_targets(targets_array) \
    dispatcher_fill_targets_impl((targets_array), sizeof(targets_array) / sizeof((targets_array)[0]))

typedef void (*dispatcher_handler_t)(const dispatcher_msg_t *msg);

void dispatcher_init(void);
// Deprecated: use dispatcher_pool_send_ptr() and pointer queues instead.
void dispatcher_send(const dispatcher_msg_t *msg);
// Deprecated: use ISR -> queue -> task -> dispatcher_pool_send_ptr().
void dispatcher_send_from_isr(const dispatcher_msg_t *msg,
                              BaseType_t *hp_task_woken);
void dispatcher_register_handler(dispatch_target_t target,
                                 dispatcher_handler_t handler);

void dispatcher_register_ptr_queue(dispatch_target_t target, QueueHandle_t queue);
int dispatcher_broadcast_ptr(pool_msg_t *msg, const dispatch_target_t *targets);
bool dispatcher_has_ptr_queue(dispatch_target_t target);

/* Return the registered pointer queue for a given target, or NULL if none. */
QueueHandle_t dispatcher_get_ptr_queue(dispatch_target_t target);

#endif // DISPATCHER_H