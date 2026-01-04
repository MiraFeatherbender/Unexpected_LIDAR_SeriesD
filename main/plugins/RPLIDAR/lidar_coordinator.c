#include <string.h>
#include "dispatcher.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lidar_message_builder.h"

#define LIDAR_TASK_STACK_SIZE 4096
#define LIDAR_TASK_PRIORITY   8
#define LIDAR_CMD_QUEUE_LEN   10

static QueueHandle_t lidar_cmd_queue = NULL;

// Forward declarations
static void lidar_task(void *arg);
static void lidar_dispatcher_handler(const dispatcher_msg_t *msg);

// Initialization function
void lidar_coordinator_init(void)
{
	// Create command queue
	lidar_cmd_queue = xQueueCreate(LIDAR_CMD_QUEUE_LEN, sizeof(dispatcher_msg_t));

	// Register dispatcher handler
	dispatcher_register_handler(TARGET_LIDAR_COORD, lidar_dispatcher_handler);

	// Start LIDAR task
	xTaskCreate(lidar_task, "lidar_task", LIDAR_TASK_STACK_SIZE, NULL, LIDAR_TASK_PRIORITY, NULL);
}

// Dispatcher handler — messages routed TO LIDAR
static void lidar_dispatcher_handler(const dispatcher_msg_t *msg)
{
	// Non-blocking enqueue
	xQueueSend(lidar_cmd_queue, msg, 0);
}

// LIDAR task — processes incoming commands
static void lidar_task(void *arg)
{
	dispatcher_msg_t msg;

	while (1) {
		// Wait for incoming command
		if (xQueueReceive(lidar_cmd_queue, &msg, portMAX_DELAY) == pdTRUE) {

			dispatcher_msg_t out_msg = {0};
			out_msg.source = SOURCE_LIDAR_COORD;

			switch(msg.source) {
				case SOURCE_USB:
					// Handle commands from USB (e.g., send Get Health to LIDAR)
					out_msg.target = TARGET_LIDAR_IO;
					out_msg.message_len = lidar_build_get_health_cmd(out_msg.data, sizeof(out_msg.data));
					dispatcher_send(&out_msg);
					break;
				case SOURCE_LIDAR_IO:
					// Handle responses from LIDAR IO (if needed)
					out_msg.target = TARGET_USB;
					out_msg.message_len = msg.message_len;
					memcpy(out_msg.data, msg.data, msg.message_len);
					dispatcher_send(&out_msg);
					break;
				default:
					// Unknown source
					break;
			}
		}
	}
}
