#include "dispatcher.h"
#include "dispatcher_pool.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static QueueHandle_t dispatcher_ptr_queues[TARGET_MAX] = { NULL };

void dispatcher_init(void)
{
    // Pointer-only dispatcher: no value-queue task initialization.
}
void dispatcher_send(const dispatcher_msg_t *msg)
{
    (void)msg;
    // Deprecated: use dispatcher_pool_send_ptr() and pointer queues instead.
    // Intentionally no-op to prevent value-path usage.
    // Keeping symbol for compatibility.
    // ESP_LOGW("dispatcher", "dispatcher_send is deprecated");
}

void dispatcher_send_from_isr(const dispatcher_msg_t *msg,
                              BaseType_t *hp_task_woken)
{
    (void)msg;
    if (hp_task_woken) {
        *hp_task_woken = pdFALSE;
    }
    // Deprecated: use ISR -> queue -> task -> dispatcher_pool_send_ptr().
    // Intentionally no-op to prevent value-path usage.
}

void dispatcher_register_handler(dispatch_target_t target,
                                 dispatcher_handler_t handler)
{
    (void)target;
    (void)handler;
    // Deprecated: pointer-only dispatcher does not use value handlers.
}

void dispatcher_register_ptr_queue(dispatch_target_t target, QueueHandle_t queue)
{
    if (target < TARGET_MAX) {
        dispatcher_ptr_queues[target] = queue;
    }
}

QueueHandle_t dispatcher_get_ptr_queue(dispatch_target_t target)
{
    if (target >= TARGET_MAX) return NULL;
    return dispatcher_ptr_queues[target];
}

int dispatcher_broadcast_ptr(pool_msg_t *msg, const dispatch_target_t *targets)
{
    if (!msg || !targets) return 0;

    int success = 0;
    for (int i = 0; i < TARGET_MAX; ++i) {
        dispatch_target_t target = targets[i];
        if (target == TARGET_MAX) continue;
        if (target >= TARGET_MAX) continue;
        QueueHandle_t q = dispatcher_ptr_queues[target];
        if (!q) continue;
        // Increment ref for this recipient before making the message visible to avoid
        // a race where the recipient unrefs before we increment and returns the
        // message back to the pool leading to a double-unref later.
        dispatcher_pool_msg_ref(msg);
        if (xQueueSend(q, &msg, 0) == pdTRUE) {
            success++;
        } else {
            // Queue full or send failed; undo the ref increment
            dispatcher_pool_msg_unref(msg);
        }
    }

    dispatcher_pool_msg_unref(msg);
    return success;
}

bool dispatcher_has_ptr_queue(dispatch_target_t target)
{
    if (target >= TARGET_MAX) return false;
    return dispatcher_ptr_queues[target] != NULL;
}

