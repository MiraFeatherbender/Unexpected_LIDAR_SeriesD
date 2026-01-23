#include "dispatcher_module.h"
#include <stdio.h>
#include <string.h>

static void dispatcher_module_ptr_task(void *arg) {
    dispatcher_module_t *module = (dispatcher_module_t *)arg;
    const char *name = module && module->name ? module->name : "dispatcher_module";

    if (!module) {
        vTaskDelete(NULL);
        return;
    }

    if (!module->queue) {
        ESP_LOGE(name, "No queue available for module; ensure queue created/registered");
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        TickType_t timeout = portMAX_DELAY;
        if (module->step_ms > 0) {
            if (module->next_step == 0) {
                module->next_step = xTaskGetTickCount() + pdMS_TO_TICKS(module->step_ms);
            }
            TickType_t now = xTaskGetTickCount();
            timeout = (module->next_step > now) ? (module->next_step - now) : 0;
        }

        /* Queue-depth warning: if queue fills above 75% warn, rate-limited to 10s */
        UBaseType_t qcount = uxQueueMessagesWaiting(module->queue);
        UBaseType_t qlen = module->queue_len;
        if (qlen > 0 && qcount >= (qlen * 3 / 4)) {
            TickType_t now = xTaskGetTickCount();
            if ((int32_t)(now - module->last_queue_warn) >= (int32_t)pdMS_TO_TICKS(10000)) {
                ESP_LOGW(name, "queue depth high: %u/%u", (unsigned)qcount, (unsigned)qlen);
                module->last_queue_warn = now;
            }
        }

        pool_msg_t *pmsg = NULL;
        if (xQueueReceive(module->queue, &pmsg, timeout) == pdTRUE) {
            dispatcher_module_process_ptr_compat(module, pmsg);
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

BaseType_t dispatcher_module_start(dispatcher_module_t *module) {
    if (!module) return pdFALSE;

    const char *name = module->name ? module->name : "dispatcher_module";

    if (!module->queue) {
        module->queue = dispatcher_ptr_queue_create_register(module->target, module->queue_len);
        if (!module->queue) {
            ESP_LOGE(name, "Failed to create pointer queue (len=%u)", (unsigned)module->queue_len);
            return pdFALSE;
        }
    }

    module->next_step = 0;
    module->last_queue_warn = 0;

    char task_name[16] = {0};
    snprintf(task_name, sizeof(task_name), "%s_ptr", name);

    BaseType_t ok = xTaskCreate(dispatcher_module_ptr_task, task_name, module->stack_size, module, module->task_prio, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(name, "Failed to create pointer task (stack=%u)", (unsigned)module->stack_size);
        return pdFALSE;
    }

    ESP_LOGI(name, "Module started (stack=%u, queue_len=%u, step_ms=%u)", (unsigned)module->stack_size, (unsigned)module->queue_len, (unsigned)module->step_ms);
    return pdTRUE;
}