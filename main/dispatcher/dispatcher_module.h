#ifndef DISPATCHER_MODULE_H
#define DISPATCHER_MODULE_H

#include "dispatcher.h"
#include "dispatcher_pool.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"

typedef void (*dispatcher_module_process_msg_t)(const dispatcher_msg_t *msg);
typedef void (*dispatcher_module_step_frame_t)(void);

typedef struct {
    const char *name;
    dispatch_target_t target;
    uint16_t queue_len;
    uint16_t stack_size;
    UBaseType_t task_prio;
    dispatcher_module_process_msg_t process_msg;
    dispatcher_module_step_frame_t step_frame;
    uint32_t step_ms;
    QueueHandle_t queue;
    TickType_t next_step;
    /* Tick count of last queue-depth warning, used to rate-limit warnings */
    TickType_t last_queue_warn;
} dispatcher_module_t;

static inline QueueHandle_t dispatcher_ptr_queue_create_register(dispatch_target_t target, uint16_t queue_len) {
    QueueHandle_t queue_handle = xQueueCreate(queue_len, sizeof(pool_msg_t *));
    if (queue_handle) {
        dispatcher_register_ptr_queue(target, queue_handle);
    }
    return queue_handle;
}

static inline void dispatcher_module_process_ptr_compat(dispatcher_module_t *module, pool_msg_t *pmsg) {
    if (!module || !pmsg) return;
    const dispatcher_msg_ptr_t *p = dispatcher_pool_get_msg_const(pmsg);
    if (!p) {
        dispatcher_pool_msg_unref(pmsg);
        return;
    }

    dispatcher_msg_t tmp = {0};
    tmp.source = p->source;
    memcpy(tmp.targets, p->targets, sizeof(tmp.targets));
    tmp.message_len = p->message_len;
    tmp.context = p->context;

    size_t copy_len = p->message_len;
    if (copy_len > BUF_SIZE) copy_len = BUF_SIZE;
    if (p->data && copy_len > 0) {
        memcpy(tmp.data, p->data, copy_len);
    }

    if (module->process_msg) module->process_msg(&tmp);
    dispatcher_pool_msg_unref(pmsg);
}

/*
 * Start a dispatcher module: create & register the pointer queue (if not set),
 * initialize timing state, and spawn the standardized pointer-task that
 * drives message unwrapping and step_frame scheduling.
 * Returns pdTRUE on success, pdFALSE on failure.
 */
BaseType_t dispatcher_module_start(dispatcher_module_t *module);

#endif // DISPATCHER_MODULE_H
