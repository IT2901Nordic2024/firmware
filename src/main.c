/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <net/aws_iot.h>
#include <stdio.h>
#include <stdlib.h>
#include <hw_id.h>
#include <modem/modem_info.h>

#include "json_payload.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

/*Settings and NVS*/
#include <zephyr/settings/settings.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/device.h>
#include <string.h>
#include <inttypes.h>
#include <zephyr/sys/printk.h>

/*Protobuf*/
#include <pb.h>
#include <pb_common.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include <src/config.pb.h>


#define SW0_NODE	DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios,
							      {0});
static struct gpio_callback button_cb_data;

#include "../ext_sensors/ext_sensors.h"

/* Register log module */
LOG_MODULE_REGISTER(dodd, CONFIG_AWS_IOT_SAMPLE_LOG_LEVEL);

/* Macros used to subscribe to specific Zephyr NET management events. */
#define L4_EVENT_MASK	      (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
#define CONN_LAYER_EVENT_MASK (NET_EVENT_CONN_IF_FATAL_ERROR)

#define MODEM_FIRMWARE_VERSION_SIZE_MAX 50

/* Macro called upon a fatal error, reboots the device. */
#define FATAL_ERROR()                                                                              \
	LOG_ERR("Fatal error! Rebooting the device.");                                             \
	LOG_PANIC();                                                                               \
	IF_ENABLED(CONFIG_REBOOT, (sys_reboot(0)))

/*Maximum configurable sides on the device*/
#define MAX_SIDES 11

/* NVS storage configuration 
* TODO: Adjust the storage configuration. Figure out NVS_FLASH_DEVICE name.
*/
#define NVS_PARTITION		  storage_partition
#define NVS_FLASH_DEVICE 	  FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_SECTOR_SIZE       4096   
#define NVS_SECTOR_COUNT      64     
#define NVS_STORAGE_OFFSET    0x0000 
#define NVS_PARTITION_OFFSET	FIXED_PARTITION_OFFSET(NVS_PARTITION)

#define MAX_CONFIG_SIZE 1024
#define CONFIG_APP_CONFIG_SIZE 1024

// static struct nvs_fs fs = {
//     .flash_device = NVS_FLASH_DEVICE,
//     .sector_size = NVS_SECTOR_SIZE,
//     .sector_count = NVS_SECTOR_COUNT,
//     .offset = NVS_STORAGE_OFFSET,
// };

/* Zephyr NET management event callback structures. */
static struct net_mgmt_event_callback l4_cb;
static struct net_mgmt_event_callback conn_cb;

/* Forward declarations. */
static void shadow_update_work_fn(struct k_work *work);
static void connect_work_fn(struct k_work *work);
static void aws_iot_event_handler(const struct aws_iot_evt *const evt);

/* Work items used to control some aspects of the sample. */
static K_WORK_DELAYABLE_DEFINE(shadow_update_work, shadow_update_work_fn);
static K_WORK_DELAYABLE_DEFINE(connect_work, connect_work_fn);

/* Static functions */

static int app_topics_subscribe(void)
{
	int err;
	static const char custom_topic[] = "my-custom-topic/example";
	static const char custom_topic_2[] = "my-custom-topic/example_2";

	const struct aws_iot_topic_data topics_list[CONFIG_AWS_IOT_APP_SUBSCRIPTION_LIST_COUNT] = {
		[0].str = custom_topic,
		[0].len = strlen(custom_topic),
		[1].str = custom_topic_2,
		[1].len = strlen(custom_topic_2)};

	err = aws_iot_subscription_topics_add(topics_list, ARRAY_SIZE(topics_list));
	if (err) {
		LOG_ERR("aws_iot_subscription_topics_add, error: %d", err);
		FATAL_ERROR();
		return err;
	}

	return 0;
}

static int aws_iot_client_init(void)
{
	int err;
	struct aws_iot_config config = {0};

#if defined(CONFIG_AWS_IOT_SAMPLE_DEVICE_ID_USE_HW_ID)
	char device_id[HW_ID_LEN] = {0};

	/* Get unique hardware ID, can be used as AWS IoT MQTT broker device/client ID. */
	err = hw_id_get(device_id, ARRAY_SIZE(device_id));
	if (err) {
		LOG_ERR("Failed to retrieve device ID, error: %d", err);
		FATAL_ERROR();
		return err;
	}

	/* To use HW ID as MQTT device/client ID set the CONFIG_AWS_IOT_CLIENT_ID_APP option.
	 * Otherwise the ID set by CONFIG_AWS_IOT_CLIENT_ID_STATIC is used.
	 */
	config.client_id = device_id;
	config.client_id_len = strlen(device_id);

	LOG_INF("Hardware ID: %s", device_id);
#endif /* CONFIG_AWS_IOT_SAMPLE_DEVICE_ID_USE_HW_ID */

	err = aws_iot_init(&config, aws_iot_event_handler);
	if (err) {
		LOG_ERR("AWS IoT library could not be initialized, error: %d", err);
		FATAL_ERROR();
		return err;
	}

	/* Add application specific non-shadow topics to the AWS IoT library.
	 * These topics will be subscribed to when connecting to the broker.
	 */
	err = app_topics_subscribe();
	if (err) {
		LOG_ERR("Adding application specific topics failed, error: %d", err);
		FATAL_ERROR();
		return err;
	}

	return 0;
}

static void shadow_update_work_fn(struct k_work *work)
{
	int err;
	char message[CONFIG_AWS_IOT_SAMPLE_JSON_MESSAGE_SIZE_MAX] = {0};
	struct payload payload = {
		.state.reported.uptime = k_uptime_get(),
		.state.reported.app_version = CONFIG_AWS_IOT_SAMPLE_APP_VERSION,
	};
	struct aws_iot_data tx_data = {
		.qos = MQTT_QOS_0_AT_MOST_ONCE,
		.topic.type = AWS_IOT_SHADOW_TOPIC_UPDATE,
	};

	if (IS_ENABLED(CONFIG_MODEM_INFO)) {
		char modem_version_temp[MODEM_FIRMWARE_VERSION_SIZE_MAX];

		err = modem_info_get_fw_version(modem_version_temp, ARRAY_SIZE(modem_version_temp));
		if (err) {
			LOG_ERR("modem_info_get_fw_version, error: %d", err);
			FATAL_ERROR();
			return;
		}

		payload.state.reported.modem_version = modem_version_temp;
	}

	err = json_payload_construct(message, sizeof(message), &payload);
	if (err) {
		LOG_ERR("json_payload_construct, error: %d", err);
		FATAL_ERROR();
		return;
	}

	tx_data.ptr = message;
	tx_data.len = strlen(message);

	LOG_INF("Publishing message: %s to AWS IoT shadow", message);

	err = aws_iot_send(&tx_data);
	if (err) {
		LOG_ERR("aws_iot_send, error: %d", err);
		FATAL_ERROR();
		return;
	}
}

/* System Workqueue handlers. */
void button_pressed()
{
	printk("Button pressed %" PRIu32 "\n", k_cycle_get_32());
	/* send shadow_update_work in front of the queue */
	(void)k_work_reschedule(&shadow_update_work, K_NO_WAIT);
}


static void connect_work_fn(struct k_work *work)
{
	int err;

	LOG_INF("Connecting to AWS IoT");

	err = aws_iot_connect(NULL);
	if (err) {
		LOG_ERR("aws_iot_connect, error: %d", err);
	}

	LOG_INF("Next connection retry in %d seconds",
		CONFIG_AWS_IOT_SAMPLE_CONNECTION_RETRY_TIMEOUT_SECONDS);

	(void)k_work_reschedule(&connect_work,
				K_SECONDS(CONFIG_AWS_IOT_SAMPLE_CONNECTION_RETRY_TIMEOUT_SECONDS));
}

/* Functions that are executed on specific connection-related events. */

static void on_aws_iot_evt_connected(const struct aws_iot_evt *const evt)
{
	(void)k_work_cancel_delayable(&connect_work);

	/* If persistent session is enabled, the AWS IoT library will not subscribe to any topics.
	 * Topics from the last session will be used.
	 */
	if (evt->data.persistent_session) {
		LOG_WRN("Persistent session is enabled, using subscriptions "
			"from the previous session");
	}

  /* set button pressed as buttons funcion */
	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
	printk("Set up button at %s pin %d\n", button.port->name, button.pin);

	/* Start sequential updates to AWS IoT */
	/*(void)k_work_reschedule(&shadow_update_work, K_NO_WAIT);*/
}

static void on_aws_iot_evt_disconnected(void)
{
	/* Remove button callback */
	gpio_remove_callback(button.port, &button_cb_data);
	printk("Button at %s pin %d has been deactivated\n", button.port->name, button.pin);
	(void)k_work_cancel_delayable(&shadow_update_work);
	(void)k_work_reschedule(&connect_work, K_SECONDS(5));
}

static void on_aws_iot_evt_fota_done(const struct aws_iot_evt *const evt)
{
	int err;

	/* Tear down MQTT connection. */
	(void)aws_iot_disconnect();
	(void)k_work_cancel_delayable(&connect_work);

	/* If modem FOTA has been carried out, the modem needs to be reinitialized.
	 * This is carried out by bringing the network interface down/up.
	 */
	if (evt->data.image & DFU_TARGET_IMAGE_TYPE_ANY_MODEM) {
		LOG_INF("Modem FOTA done, reinitializing the modem");

		err = conn_mgr_all_if_down(true);
		if (err) {
			LOG_ERR("conn_mgr_all_if_down, error: %d", err);
			FATAL_ERROR();
			return;
		}

		err = conn_mgr_all_if_up(true);
		if (err) {
			LOG_ERR("conn_mgr_all_if_up, error: %d", err);
			FATAL_ERROR();
			return;
		}

	} else if (evt->data.image & DFU_TARGET_IMAGE_TYPE_ANY_APPLICATION) {
		LOG_INF("Application FOTA done, rebooting");
		IF_ENABLED(CONFIG_REBOOT, (sys_reboot(0)));
	} else {
		LOG_WRN("Unexpected FOTA image type");
	}
}

static void on_net_event_l4_connected(void)
{
	(void)k_work_reschedule(&connect_work, K_SECONDS(5));
}

static void on_net_event_l4_disconnected(void)
{
	(void)aws_iot_disconnect();
	(void)k_work_cancel_delayable(&connect_work);
	(void)k_work_cancel_delayable(&shadow_update_work);
}

void save_config(const char *encoded_message, size_t message_length) {
	// Decode the configuration from the protobuf
	Config config = Config_init_zero;
	pb_istream_t stream = pb_istream_from_buffer((const pb_byte_t *)encoded_message, message_length);
	bool status = pb_decode(&stream, Config_fields, &config);
	if (!status) {
		// Handle decoding error 
		return;
	}
	// Print the decoded int32 id
	LOG_INF("Received configuration with id: %d", config.id);
	// Print the decoded int32 timestamp
	LOG_INF("Received configuration with timestamp: %d", config.timestamp);

	// Save the configuration to NVS using Settings API
	// int err;
	// char key[20]; // Assuming a maximum length for the key
	// snprintf(key, sizeof(key), "side%d_config", config.side);

	// // Write configuration to NVS
	// err = settings_save_one(key, &config, sizeof(config));
	// if (err >= 0) {
	// 	// Configuration saved successfully
	// 	printk("Configuration for side %d saved successfully\n", config.id);
	// } else {
	// 	// Error occurred while saving configuration
	// 	printk("Error saving configuration for side %d: %d\n", config.id, err);
	// }

}
/* Event handlers */

static void aws_iot_event_handler(const struct aws_iot_evt *const evt)
{
	switch (evt->type) {
	case AWS_IOT_EVT_CONNECTING:
		LOG_INF("AWS_IOT_EVT_CONNECTING");
		break;
	case AWS_IOT_EVT_CONNECTED:
		LOG_INF("AWS_IOT_EVT_CONNECTED");
		on_aws_iot_evt_connected(evt);
		break;
	case AWS_IOT_EVT_READY:
		LOG_INF("AWS_IOT_EVT_READY");
		break;
	case AWS_IOT_EVT_DISCONNECTED:
		LOG_INF("AWS_IOT_EVT_DISCONNECTED");
		on_aws_iot_evt_disconnected();
		break;
	case AWS_IOT_EVT_DATA_RECEIVED:
		LOG_INF("AWS_IOT_EVT_DATA_RECEIVED");
		save_config(evt->data.msg.topic.str);
		LOG_INF("Received message: \"%.*s\" on topic: \"%.*s\"", evt->data.msg.len,
			evt->data.msg.ptr, evt->data.msg.topic.len, evt->data.msg.topic.str);
		break;
	case AWS_IOT_EVT_PUBACK:
		LOG_INF("AWS_IOT_EVT_PUBACK, message ID: %d", evt->data.message_id);
		break;
	case AWS_IOT_EVT_PINGRESP:
		LOG_INF("AWS_IOT_EVT_PINGRESP");
		break;
	case AWS_IOT_EVT_FOTA_START:
		LOG_INF("AWS_IOT_EVT_FOTA_START");
		break;
	case AWS_IOT_EVT_FOTA_ERASE_PENDING:
		LOG_INF("AWS_IOT_EVT_FOTA_ERASE_PENDING");
		break;
	case AWS_IOT_EVT_FOTA_ERASE_DONE:
		LOG_INF("AWS_FOTA_EVT_ERASE_DONE");
		break;
	case AWS_IOT_EVT_FOTA_DONE:
		LOG_INF("AWS_IOT_EVT_FOTA_DONE");
		on_aws_iot_evt_fota_done(evt);
		break;
	case AWS_IOT_EVT_FOTA_DL_PROGRESS:
		LOG_INF("AWS_IOT_EVT_FOTA_DL_PROGRESS, (%d%%)", evt->data.fota_progress);
		break;
	case AWS_IOT_EVT_ERROR:
		LOG_INF("AWS_IOT_EVT_ERROR, %d", evt->data.err);
		break;
	case AWS_IOT_EVT_FOTA_ERROR:
		LOG_INF("AWS_IOT_EVT_FOTA_ERROR");
		break;
	default:
		LOG_WRN("Unknown AWS IoT event type: %d", evt->type);
		break;
	}
}

static void l4_event_handler(struct net_mgmt_event_callback *cb, uint32_t event,
			     struct net_if *iface)
{
	switch (event) {
	case NET_EVENT_L4_CONNECTED:
		LOG_INF("Network connectivity established");
		on_net_event_l4_connected();
		break;
	case NET_EVENT_L4_DISCONNECTED:
		LOG_INF("Network connectivity lost");
		on_net_event_l4_disconnected();
		break;
	default:
		/* Don't care */
		return;
	}
}

static void connectivity_event_handler(struct net_mgmt_event_callback *cb, uint32_t event,
				       struct net_if *iface)
{
	if (event == NET_EVENT_CONN_IF_FATAL_ERROR) {
		LOG_ERR("NET_EVENT_CONN_IF_FATAL_ERROR");
		FATAL_ERROR();
		return;
	}
}

/* Load and save configuration of side
* DEPENDS ON SETTINGS AND NVS
* DEPENDS ON PROTOBUF
* TODO: Implement protobuf
*/

// Receive protobuf from mqtt topic and call save_config



// void load_config() {
//     int err;
//     uint8_t config_buf[MAX_CONFIG_SIZE]; // Assuming a maximum size for the configuration buffer
//     size_t config_size;

//     // Iterate over each side and load its configuration
//     for (int side = 0; side < MAX_SIDES; side++) {
//         char key[20]; // Assuming a maximum length for the key
//         snprintf(key, sizeof(key), "side%d_config", side);

//         // Read configuration from NVS
//         err = settings_load_subtree(key, config_buf, sizeof(config_buf));
//         if (err >= 0) {
//             config_size = err;
//             // Configuration found, process it (e.g., deserialize from protobuf)
//             // Example: process_config(side, config_buf, config_size);
//             printk("Configuration for side %d loaded successfully\n", side);
//         } else if (err == -ENOENT) {
//             // Configuration not found for this side
//             printk("No configuration found for side %d\n", side);
//         } else {
//             // Error occurred while reading configuration
//             printk("Error reading configuration for side %d: %d\n", side, err);
//         }
//     }
// }



int main(void)
{

	/* init button, when button is pressed call function button-pressed */
	int ret;

	if (!gpio_is_ready_dt(&button)) {
		printk("Error: button device %s is not ready\n",
		       button.port->name);
		return 0;
	}

	LOG_INF("The AWS IoT sample started, version: %s", CONFIG_AWS_IOT_SAMPLE_APP_VERSION);

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n",
		       ret, button.port->name, button.pin);
		return 0;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, button.port->name, button.pin);
		return 0;
	}

	/* init the aws connection */
	int err;

	/* Setup handler for Zephyr NET Connection Manager events. */
	net_mgmt_init_event_callback(&l4_cb, l4_event_handler, L4_EVENT_MASK);
	net_mgmt_add_event_callback(&l4_cb);

	/* Setup handler for Zephyr NET Connection Manager Connectivity layer. */
	net_mgmt_init_event_callback(&conn_cb, connectivity_event_handler, CONN_LAYER_EVENT_MASK);
	net_mgmt_add_event_callback(&conn_cb);

	/* Connecting to the configured connectivity layer. */
	LOG_INF("Bringing network interface up and connecting to the network");

	err = conn_mgr_all_if_up(true);
	if (err) {
		LOG_ERR("conn_mgr_all_if_up, error: %d", err);
		FATAL_ERROR();
		return err;
	}

	err = aws_iot_client_init();
	if (err) {
		LOG_ERR("aws_iot_client_init, error: %d", err);
		FATAL_ERROR();
		return err;
	}

	/* Resend connection status if the sample is built for QEMU x86.
	 * This is necessary because the network interface is automatically brought up
	 * at SYS_INIT() before main() is called.
	 * This means that NET_EVENT_L4_CONNECTED fires before the
	 * appropriate handler l4_event_handler() is registered.
	 */
	if (IS_ENABLED(CONFIG_BOARD_QEMU_X86)) {
		conn_mgr_mon_resend_status();
	}

	const char encoded_message[] = "\x08\xb9`\x10\x02\x18\x03 \x01";
	size_t message_length = sizeof(encoded_message) - 1; // Exclude the null terminator
	save_config(encoded_message, message_length);

	return 0;
}
