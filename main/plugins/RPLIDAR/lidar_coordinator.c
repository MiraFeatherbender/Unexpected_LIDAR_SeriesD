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
#include "esp_log.h"

#define LIDAR_TASK_STACK_SIZE 4096
#define LIDAR_TASK_PRIORITY   8
#define LIDAR_CMD_QUEUE_LEN   10

static QueueHandle_t lidar_ptr_queue = NULL;

// Forward declarations
static void lidar_task(void *arg);

/* Small send helper: single place to add logging/metrics/retries later */
static inline void lidar_send(const dispatcher_pool_send_params_t *params)
{
	dispatcher_pool_send_ptr_params(params);
}

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
			}				dispatcher_msg_t out_msg = {0};
				lidar_response_desc_t resp_desc = {0};
				out_msg.source = SOURCE_LIDAR_COORD;
				bool pmsg_unrefed = false;
				dispatcher_fill_targets(out_msg.targets);

				/* base params template for outgoing CONTROL messages; cases will set .data/.data_len as needed */
				dispatcher_pool_send_params_t base = {
					.type = DISPATCHER_POOL_CONTROL,
					.source = SOURCE_LIDAR_COORD,
					.targets = out_msg.targets,
					.data = NULL,
					.data_len = 0,
					.context = NULL
				};
			switch(in->source) {

					case SOURCE_LIDAR_IO: {
						// Handle responses from LIDAR IO (if needed)
						   out_msg.targets[0] = TARGET_LOG;
						   {
							size_t in_len = in->message_len;
						uint8_t local_in_buf[256] = {0};
							size_t copy_len = (in_len < sizeof(local_in_buf)) ? in_len : sizeof(local_in_buf);
							if (copy_len > 0 && in->data) memcpy(local_in_buf, in->data, copy_len);

							/* Early unref the incoming pointer message */
							dispatcher_pool_msg_unref(pmsg);
							pmsg_unrefed = true;
							
						if (local_in_buf[0] != LIDAR_RSP_SYNC_BYTE1 || local_in_buf[1] != LIDAR_RSP_SYNC_BYTE2) {
							ESP_LOGI("lidar_coord", "LIDAR response invalid header: %02X %02X",
										(unsigned)local_in_buf[0], (unsigned)local_in_buf[1]);
							// Invalid, ignore
							break;
						} 
						// Parse header from local copy
						resp_desc.payload_len = (local_in_buf[2]) | (local_in_buf[3] << 8) | (local_in_buf[4] << 16);
						resp_desc.response_type = local_in_buf[6];
						size_t available = (copy_len > 7) ? (copy_len - 7) : 0;
						if (resp_desc.payload_len > available) resp_desc.payload_len = available;
						resp_desc.payload = &local_in_buf[7];

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
							base.targets = out_msg.targets;
							base.data = (const uint8_t *)usb_buf;
							base.data_len = out_msg.message_len;
							lidar_send(&base);
							handled = true;
							}
						}
						if (!handled) {
							// Fallback: forward raw payload to USB (from local copy)
							out_msg.message_len = (uint16_t)copy_len;
							base.targets = out_msg.targets;
							base.data = local_in_buf;
							base.data_len = out_msg.message_len;
							lidar_send(&base);
								}
							}
						break;
					}
				default: {
					// Treat any other source as a control request and forward GET_INFO to the LIDAR
					out_msg.targets[0] = TARGET_LIDAR_IO;
					out_msg.message_len = lidar_build_by_idx(out_msg.data, sizeof(out_msg.data), LIDAR_CMD_IDX_GET_INFO);
					base.targets = out_msg.targets;
					base.data = out_msg.data;
					base.data_len = out_msg.message_len;
					lidar_send(&base);
						break;
				}
			}
			if (!pmsg_unrefed) dispatcher_pool_msg_unref(pmsg);
		}
	}
}
