#ifndef DISPATCHER_MODULE_H
#define DISPATCHER_MODULE_H

#include "dispatcher.h"
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
} dispatcher_module_t;

static inline void dispatcher_module_task(void *arg) {
    dispatcher_module_t *module = (dispatcher_module_t *)arg;
    if (!module || !module->queue) {
        if (module && module->name) {
            ESP_LOGE("dispatcher_module", "Queue not initialized for %s", module->name);
        } else {
            ESP_LOGE("dispatcher_module", "Queue not initialized for module");
        }
        vTaskDelete(NULL);
        return;
    }
    dispatcher_msg_t msg = {0};
    while (1) {
        TickType_t timeout = portMAX_DELAY;
        if (module->step_ms > 0) {
            if (module->next_step == 0) {
                module->next_step = xTaskGetTickCount() + pdMS_TO_TICKS(module->step_ms);
            }
            TickType_t now = xTaskGetTickCount();
            timeout = (module->next_step > now) ? (module->next_step - now) : 0;
        }

        if (xQueueReceive(module->queue, &msg, timeout) == pdTRUE) {
            if (module->process_msg) module->process_msg(&msg);
        }

        if (module->step_frame && module->step_ms > 0) {
            TickType_t now = xTaskGetTickCount();
            if ((int32_t)(now - module->next_step) >= 0) {
                module->step_frame();
                module->next_step = now + pdMS_TO_TICKS(module->step_ms);
            }
        }
    }
}

static inline void dispatcher_module_enqueue(dispatcher_module_t *module, const dispatcher_msg_t *msg) {
    if (!module || !module->queue) return;
    xQueueSend(module->queue, msg, 0);
}

static inline void dispatcher_module_start(dispatcher_module_t *module, dispatcher_handler_t handler) {
    module->queue = xQueueCreate(module->queue_len, sizeof(dispatcher_msg_t));
    if (!module->queue) {
        if (module->name) {
            ESP_LOGE("dispatcher_module", "Failed to create queue for %s", module->name);
        } else {
            ESP_LOGE("dispatcher_module", "Failed to create queue for module");
        }
        return;
    }
    module->next_step = 0;
    dispatcher_register_handler(module->target, handler);
    if (xTaskCreate(dispatcher_module_task, module->name, module->stack_size, module, module->task_prio, NULL) != pdPASS) {
        ESP_LOGE("dispatcher_module", "Failed to create task for %s", module->name ? module->name : "module");
    }
}

static inline void dispatcher_module_start_custom(dispatcher_module_t *module, dispatcher_handler_t handler) {
    module->queue = xQueueCreate(module->queue_len, sizeof(dispatcher_msg_t));
    if (!module->queue) {
        if (module->name) {
            ESP_LOGE("dispatcher_module", "Failed to create queue for %s", module->name);
        } else {
            ESP_LOGE("dispatcher_module", "Failed to create queue for module");
        }
        return;
    }
    module->next_step = 0;
    dispatcher_register_handler(module->target, handler);
}

#endif // DISPATCHER_MODULE_H
