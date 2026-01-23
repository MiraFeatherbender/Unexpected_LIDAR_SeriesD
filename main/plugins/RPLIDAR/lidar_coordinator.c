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

static QueueHandle_t lidar_ptr_queue = NULL;

// Forward declarations
static void lidar_task(void *arg);

// Initialization function
void lidar_coordinator_init(void)
{
	lidar_response_parser_init();

	// Register pointer queue
	lidar_ptr_queue = xQueueCreate(LIDAR_CMD_QUEUE_LEN, sizeof(pool_msg_t *));
	if (lidar_ptr_queue) {
		dispatcher_register_ptr_queue(TARGET_LIDAR_COORD, lidar_ptr_queue);
	}

	// Start LIDAR task
	xTaskCreate(lidar_task, "lidar_task", LIDAR_TASK_STACK_SIZE, NULL, LIDAR_TASK_PRIORITY, NULL);
}

// LIDAR task — processes incoming commands
static void lidar_task(void *arg)
{
	while (1) {
		// Wait for incoming command (pointer queue)
		pool_msg_t *pmsg = NULL;
		if (xQueueReceive(lidar_ptr_queue, &pmsg, portMAX_DELAY) == pdTRUE) {
			const dispatcher_msg_ptr_t *in = dispatcher_pool_get_msg_const(pmsg);
			if (!in) {
				dispatcher_pool_msg_unref(pmsg);
				continue;
			}

			dispatcher_msg_t out_msg = {0};
			lidar_response_desc_t resp_desc = {0};
			out_msg.source = SOURCE_LIDAR_COORD;
			dispatcher_fill_targets(out_msg.targets);
			switch(in->source) {
					   case SOURCE_USB_CDC:
						// Handle commands from USB (e.g., send Get Health to LIDAR)
						out_msg.targets[0] = TARGET_LIDAR_IO;
						out_msg.message_len = lidar_build_by_idx(out_msg.data, sizeof(out_msg.data), LIDAR_CMD_IDX_GET_INFO);
						dispatcher_pool_send_params_t params = {
							.type = DISPATCHER_POOL_CONTROL,
							.source = SOURCE_LIDAR_COORD,
							.targets = out_msg.targets,
							.data = out_msg.data,
							.data_len = out_msg.message_len,
							.context = NULL
						};
						dispatcher_pool_send_ptr_params(&params);
						break;
					case SOURCE_LIDAR_IO: {
						// Handle responses from LIDAR IO (if needed)
						   out_msg.targets[0] = TARGET_SSE_CONSOLE;
						   out_msg.targets[1] = TARGET_LOG;
						if(in->data[0] != LIDAR_RSP_SYNC_BYTE1 || in->data[1] != LIDAR_RSP_SYNC_BYTE2) {
							// Invalid response, ignore
							break;
						}
						// Parse header
						resp_desc.payload_len = (in->data[2]) | (in->data[3] << 8) | (in->data[4] << 16);
						resp_desc.response_type = in->data[6];
						resp_desc.payload = &in->data[7];

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
									dispatcher_pool_send_params_t params = {
										.type = DISPATCHER_POOL_CONTROL,
										.source = SOURCE_LIDAR_COORD,
										.targets = out_msg.targets,
										.data = (const uint8_t *)usb_buf,
										.data_len = out_msg.message_len,
										.context = NULL
									};
									dispatcher_pool_send_ptr_params(&params);
								handled = true;
							}
						}
						if (!handled) {
							// Fallback: forward raw payload to USB
							out_msg.message_len = in->message_len;
								dispatcher_pool_send_params_t params = {
									.type = DISPATCHER_POOL_CONTROL,
									.source = SOURCE_LIDAR_COORD,
									.targets = out_msg.targets,
									.data = in->data,
									.data_len = out_msg.message_len,
									.context = NULL
								};
								dispatcher_pool_send_ptr_params(&params);
						}
						break;
					}
					default:
						// Unknown source
						break;
				}
			dispatcher_pool_msg_unref(pmsg);
		}
	}
}
