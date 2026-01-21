#ifndef DISPATCHER_MODULE_H
#define DISPATCHER_MODULE_H

#include "dispatcher.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

typedef void (*dispatcher_module_on_msg_t)(const dispatcher_msg_t *msg);

typedef struct {
    const char *name;
    dispatch_target_t target;
    uint16_t queue_len;
    uint16_t stack_size;
    UBaseType_t task_prio;
    dispatcher_module_on_msg_t on_msg;
    QueueHandle_t queue;
} dispatcher_module_t;

static inline void dispatcher_module_task(void *arg) {
    dispatcher_module_t *m = (dispatcher_module_t *)arg;
    dispatcher_msg_t msg = {0};
    while (1) {
        if (xQueueReceive(m->queue, &msg, portMAX_DELAY) == pdTRUE) {
            if (m->on_msg) m->on_msg(&msg);
        }
    }
}

static inline void dispatcher_module_enqueue(dispatcher_module_t *m, const dispatcher_msg_t *msg) {
    if (!m || !m->queue) return;
    xQueueSend(m->queue, msg, 0);
}

static inline void dispatcher_module_start(dispatcher_module_t *m, dispatcher_handler_t handler) {
    m->queue = xQueueCreate(m->queue_len, sizeof(dispatcher_msg_t));
    dispatcher_register_handler(m->target, handler);
    xTaskCreate(dispatcher_module_task, m->name, m->stack_size, m, m->task_prio, NULL);
}

#endif // DISPATCHER_MODULE_H
