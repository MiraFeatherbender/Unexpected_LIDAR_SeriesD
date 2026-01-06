#include "dispatcher.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define DISPATCHER_QUEUE_LENGTH 10
#define DISPATCHER_TASK_STACK_SIZE 4096
#define DISPATCHER_TASK_PRIORITY 10

static void dispatcher_task(void *arg);

// Dispatcher queue
static QueueHandle_t dispatcher_queue = NULL;

// Handler array
static dispatcher_handler_t handlers[TARGET_MAX] = { NULL };

void dispatcher_init(void)
{
    dispatcher_queue = xQueueCreate(DISPATCHER_QUEUE_LENGTH, sizeof(dispatcher_msg_t));
    // Create dispatcher task
    xTaskCreate(dispatcher_task, "dispatcher_task", DISPATCHER_TASK_STACK_SIZE, NULL, DISPATCHER_TASK_PRIORITY, NULL);
}
void dispatcher_send(const dispatcher_msg_t *msg)
{
    if (dispatcher_queue != NULL) {
        xQueueSend(dispatcher_queue, msg, portMAX_DELAY);
    }
}

void dispatcher_send_from_isr(const dispatcher_msg_t *msg,
                              BaseType_t *hp_task_woken)
{
    if (dispatcher_queue != NULL) {
        xQueueSendFromISR(dispatcher_queue, msg, hp_task_woken);
    }
}

void dispatcher_register_handler(dispatch_target_t target,
                                 dispatcher_handler_t handler)
{
    if (target < TARGET_MAX) {
        handlers[target] = handler;
    }
}

static void dispatcher_task(void *arg)
{
    dispatcher_msg_t msg;
    while (1) {
        // Wait for incoming messages
        if (xQueueReceive(dispatcher_queue, &msg, portMAX_DELAY) == pdTRUE) {
            // Call the registered handler for the target
            for(int i = 0; i < TARGET_MAX; i++){
                if(msg.targets[i] == TARGET_MAX) continue;
                if (handlers[msg.targets[i]]) handlers[msg.targets[i]](&msg);
            }
        }
    }
}
