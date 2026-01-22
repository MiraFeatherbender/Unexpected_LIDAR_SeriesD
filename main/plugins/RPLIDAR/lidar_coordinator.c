#include <string.h>
#include "dispatcher.h"
#include "dispatcher_pool.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lidar_coordinator.h"
#include "lidar_message_builder.h"
#include "lidar_protocol_cmd.h"
#include "lidar_protocol_rsp.h"
#include "lidar_response_parser.h"

#define LIDAR_TASK_STACK_SIZE 4096
#define LIDAR_TASK_PRIORITY   8
#define LIDAR_CMD_QUEUE_LEN   10

static QueueHandle_t lidar_cmd_queue = NULL;
static QueueHandle_t lidar_ptr_queue = NULL;

// Forward declarations
static void lidar_task(void *arg);
static void lidar_dispatcher_handler(const dispatcher_msg_t *msg);
static void lidar_ptr_task(void *arg);

// Initialization function
void lidar_coordinator_init(void)
{
	lidar_response_parser_init();

	// Create command queue
	lidar_cmd_queue = xQueueCreate(LIDAR_CMD_QUEUE_LEN, sizeof(dispatcher_msg_t));

	// Register dispatcher handler
	dispatcher_register_handler(TARGET_LIDAR_COORD, lidar_dispatcher_handler);

	// Register pointer queue
	lidar_ptr_queue = xQueueCreate(LIDAR_CMD_QUEUE_LEN, sizeof(pool_msg_t *));
	if (lidar_ptr_queue) {
		dispatcher_register_ptr_queue(TARGET_LIDAR_COORD, lidar_ptr_queue);
		xTaskCreate(lidar_ptr_task, "lidar_ptr_task", LIDAR_TASK_STACK_SIZE, NULL, LIDAR_TASK_PRIORITY, NULL);
	}

	// Start LIDAR task
	xTaskCreate(lidar_task, "lidar_task", LIDAR_TASK_STACK_SIZE, NULL, LIDAR_TASK_PRIORITY, NULL);
}

// Dispatcher handler — messages routed TO LIDAR
static void lidar_dispatcher_handler(const dispatcher_msg_t *msg)
{
	// Non-blocking enqueue
	xQueueSend(lidar_cmd_queue, msg, 0);
}

// Pointer queue handler — unwraps pointer msg into value queue
static void lidar_ptr_task(void *arg)
{
	(void)arg;
	while (1) {
		pool_msg_t *pmsg = NULL;
		if (xQueueReceive(lidar_ptr_queue, &pmsg, portMAX_DELAY) == pdTRUE) {
			const dispatcher_msg_ptr_t *pm = dispatcher_pool_get_msg_const(pmsg);
			if (pm) {
				dispatcher_msg_t tmp = {0};
				tmp.source = pm->source;
				memcpy(tmp.targets, pm->targets, sizeof(tmp.targets));
				tmp.message_len = pm->message_len;
				tmp.context = pm->context;
				size_t copy_len = pm->message_len;
				if (copy_len > BUF_SIZE) copy_len = BUF_SIZE;
				if (pm->data && copy_len > 0) {
					memcpy(tmp.data, pm->data, copy_len);
				}
				xQueueSend(lidar_cmd_queue, &tmp, 0);
			}
			dispatcher_pool_msg_unref(pmsg);
		}
	}
}

// LIDAR task — processes incoming commands
static void lidar_task(void *arg)
{
	dispatcher_msg_t msg;

	while (1) {
		// Wait for incoming command
		if (xQueueReceive(lidar_cmd_queue, &msg, portMAX_DELAY) == pdTRUE) {

			dispatcher_msg_t out_msg = {0};
			lidar_response_desc_t resp_desc = {0};
			out_msg.source = SOURCE_LIDAR_COORD;
				memset(out_msg.targets, TARGET_MAX, sizeof(out_msg.targets));
				switch(msg.source) {
					   case SOURCE_USB_CDC:
						// Handle commands from USB (e.g., send Get Health to LIDAR)
						out_msg.targets[0] = TARGET_LIDAR_IO;
						out_msg.message_len = lidar_build_by_idx(out_msg.data, sizeof(out_msg.data), LIDAR_CMD_IDX_GET_INFO);
						dispatcher_pool_send_ptr(DISPATCHER_POOL_CONTROL,
										SOURCE_LIDAR_COORD,
										out_msg.targets,
										out_msg.data,
										out_msg.message_len,
										NULL);
						break;
					case SOURCE_LIDAR_IO: {
						// Handle responses from LIDAR IO (if needed)
						   out_msg.targets[0] = TARGET_SSE_CONSOLE;
						   out_msg.targets[1] = TARGET_LOG;
						if(msg.data[0] != LIDAR_RSP_SYNC_BYTE1 || msg.data[1] != LIDAR_RSP_SYNC_BYTE2) {
							// Invalid response, ignore
							break;
						}
						// Parse header
						resp_desc.payload_len = (msg.data[2]) | (msg.data[3] << 8) | (msg.data[4] << 16);
						resp_desc.response_type = msg.data[6];
						resp_desc.payload = &msg.data[7];

						bool handled = false;
						const lidar_response_parser_entry_t *entry = NULL;
						if (resp_desc.response_type < lidar_response_parser_table_size) {
							entry = lidar_response_parser_table[resp_desc.response_type];
						}
						if (entry && entry->parser && entry->formatter) {
							uint8_t parsed_buf[32] = {0}; // Adjust size as needed for largest static response
							bool ok = entry->parser(resp_desc.payload, resp_desc.payload_len, parsed_buf, entry->struct_info);
							if (ok) {
								char usb_buf[128];
								entry->formatter(parsed_buf, usb_buf, sizeof(usb_buf), entry->struct_info);
								out_msg.message_len = (uint16_t)strnlen(usb_buf, sizeof(usb_buf));
								dispatcher_pool_send_ptr(DISPATCHER_POOL_CONTROL,
											SOURCE_LIDAR_COORD,
											out_msg.targets,
											(const uint8_t *)usb_buf,
											out_msg.message_len,
											NULL);
								handled = true;
							}
						}
						if (!handled) {
							// Fallback: forward raw payload to USB
							out_msg.message_len = msg.message_len;
							dispatcher_pool_send_ptr(DISPATCHER_POOL_CONTROL,
										SOURCE_LIDAR_COORD,
										out_msg.targets,
										msg.data,
										out_msg.message_len,
										NULL);
						}
						break;
					}
					default:
						// Unknown source
						break;
				}
		}
	}
}
