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
#include "settings_defs.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util_macro.h>
#include <zephyr/drivers/sensor.h>
#include "ext_sensors.h"

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

/*CJson*/
#include <cJSON.h>


/* button */
#define SW0_NODE	DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios,
							      {0});
static struct gpio_callback button_cb_data;

typedef struct settings_data Settings_data; 

char *id_str;

struct side_item *side_items[MAX_SIDES];

int payload_side_count = 0;


uint16_t config_version;

// Settings handling for config version
int config_version_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	if (len != sizeof(config_version)) {
		return -EINVAL;
	}

	int rc = read_cb(cb_arg, &config_version, sizeof(config_version));
	if (rc >= 0) {
		return 0;
	}
	return rc;
}

struct settings_handler config_version_conf = {
	.name = "config_version",
	.h_set = config_version_set,
};

#include "../ext_sensors/ext_sensors.h"

/* Register log module */
LOG_MODULE_REGISTER(dodd, CONFIG_AWS_IOT_SAMPLE_LOG_LEVEL);

/* Macros used to subscribe to specific Zephyr NET management events. */
#define L4_EVENT_MASK	      (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
#define CONN_LAYER_EVENT_MASK (NET_EVENT_CONN_IF_FATAL_ERROR)

#define MODEM_FIRMWARE_VERSION_SIZE_MAX 50

/* Macro called upon a fatal error, reboots the device. */
#define FATAL_ERROR()                                                                        \
	LOG_ERR("Fatal error! Rebooting the device.");                                             \
	LOG_PANIC();                                                                               \
	IF_ENABLED(CONFIG_REBOOT, (sys_reboot(0)))

/* additional definitions */

/* 
* Sensor variables
*/
/* Sensor device */
static const struct device *sensor = DEVICE_DT_GET(DT_NODELABEL(adxl362));
/* Sensor channels */
static const enum sensor_channel channels[] = {
	SENSOR_CHAN_ACCEL_X,
	SENSOR_CHAN_ACCEL_Y,
	SENSOR_CHAN_ACCEL_Z,
};
/* Sensor data */
struct sensor_value accel[3];
char accelX[10];
char accelY[10];
char accelZ[10];

/* counter variables */
int occurrence_count = 0;

/* LED */
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static struct k_work_delayable led_off_work;

/* Side of the device for sending based on rotation */
int side;
int newSide;

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
static void turn_led_off(struct k_work *work)
{
    gpio_pin_set_dt(&led, 0); // Assuming "0" turns the LED off.
}

static int fetch_accels(const struct device *dev)
{
	/*
	*	Fetch sensor data from the accelerometer sensor
	*	saves and prints the sensor data as string to be sent to AWS IoT
	*/
	int ret;
	/*	Check if device is ready, if not return 0	*/
	if (!device_is_ready(dev)) {
		printk("sensor: device not ready.\n");
		return 0;
	}
	else {
		printk("sensor: device ready.\n");
	}

	ret = sensor_sample_fetch(dev);
	if (ret < 0) {
		printk("sensor_sample_fetch() failed: %d\n", ret);
		return ret;
	}

	/* Get sensor data */
	for (size_t i = 0; i < ARRAY_SIZE(channels); i++) {
		ret = sensor_channel_get(dev, channels[i], &accel[i]);
		if (ret < 0) {
			printk("sensor_channel_get(%c) failed: %d\n", 'X' + i, ret);
			return ret;
		}
	}
	snprintf(accelX, sizeof(accelX), "%f", sensor_value_to_double(&accel[0]));
	snprintf(accelY, sizeof(accelY), "%f", sensor_value_to_double(&accel[1]));
	snprintf(accelZ, sizeof(accelZ), "%f", sensor_value_to_double(&accel[2]));
	// printf("Value of X: %s\n", accelX);
	// printf("Value of Y: %s\n", accelY);
	// printf("Value of Z: %s\n", accelZ);
	return 0;
}

static int get_side(const struct device *dev)
{
	/*
	* Fetch sensor data from the accelerometer sensor
	* and return the side of the device based on the z-axis
	* 1 for up and -1 for down
	*/
	int ret;
	/*	Check if device is ready, if not return 0	*/
	if (!device_is_ready(dev)) {
		printk("sensor: device not ready.\n");
		return 0;
	}

	ret = sensor_sample_fetch(dev);
	if (ret < 0) {
		printk("sensor_sample_fetch() failed: %d\n", ret);
		return ret;
	}

	/* Get sensor data */
	for (size_t i = 0; i < ARRAY_SIZE(channels); i++) {
		ret = sensor_channel_get(dev, channels[i], &accel[i]);
		if (ret < 0) {
			printk("sensor_channel_get(%c) failed: %d\n", 'X' + i, ret);
			return ret;
		}
	}

	printk("Value of z: %f\n", sensor_value_to_double(&accel[2]));
	if (sensor_value_to_double(&accel[2]) > 0) {
		printk("Side: 1\n");
		return 1;
	}
	else {
		printk("Side: -1\n");
		return -1;
	}
}

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
	char message[CONFIG_AWS_IOT_SAMPLE_JSON_MESSAGE_SIZE_MAX] = { 0 };
	// TODO: Send side config settings to AWS
	struct payload payload = {
		.version = config_version,
		.state.reported.count = payload_side_count,
		.state.reported.side_items = side_items,
	}; 


	err = json_payload_construct(message, sizeof(message), &payload);
	if (err) {
			LOG_ERR("json_payload_construct, error: %d", err);
			FATAL_ERROR();
			return;
	}

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
/* put shadow update work infornt of the kwork queue that sends event to aws */
static void event_trigger()
{
	printk("event_trigger\n");
	/* send shadow_update_work in front of the queue */
	(void)k_work_reschedule(&shadow_update_work, K_NO_WAIT);
}

/* function with a while loop that checks if side is changed */
static void check_position(void) {
   while (true) {
        newSide = get_side(sensor);
				/* if side is changed send event trigger */
        if (side != 0 && side != newSide) {
            side = newSide;
            event_trigger();
        }
        k_msleep(2000);
    }
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
	gpio_init_callback(&button_cb_data, event_trigger, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
	printk("Set up button at %s pin %d\n", button.port->name, button.pin);

	/* Start to check the position */
	// check_position();
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


static void save_side_config(int side, Settings_data side_settings){
	char name[20];
	
	sprintf(name, "side_%d/id", side);
	printk("Saving side_%d/id: %s\n", side, side_settings.id);
	int ret = settings_save_one(name, &side_settings.id, sizeof(side_settings.id));

	if (ret) {
		printk("Error saving side_%d/id: %d\n", side, ret);
	} 

	sprintf(name, "side_%d/type", side);
	printk("Saving side_%d/type: %s\n", side, side_settings.type);
	ret = settings_save_one(name, &side_settings.type, sizeof(side_settings.type));
	if (ret) {
		printk("Error saving side_%d/type: %d\n", side, ret);
	} 
	printk("Saved side_%d\n", side);
	printk("Saved side_%d/id: %s\n", side, side_settings.id);
	printk("Saved side_%d/type: %s\n", side, side_settings.type);
	// Iterate over id string and print each char individually
	for (int i = 0; i < strlen(side_settings.id); i++) {
		printk("Char %d: %c\n", i, side_settings.id[i]);
	}
}

static int start_settings_subsystem(){
	int err = settings_subsys_init();
	if (err) {
		printk("Error initializing settings subsystem: %d\n", err);
		return err;
	}
	for (int i = 0; i < MAX_SIDES; i++) {
		err = settings_register(side_confs[i]);
		if (err) {
			printk("Error registering settings for side %d: %d\n", i, err);
			return err;
		}
	}
	err = settings_register(&config_version_conf);
	if (err) {
		printk("Error registering settings for config_version: %d\n", err);
		return err;
		}
	err = settings_load();
	if (err) {
		printk("Error loading settings: %d\n", err);
		return err;
	}
	return 0;

}

static void parse_config_json(const char *json){
	printk("Parsing JSON\n");
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            printk("Error before: %s\n", error_ptr);
        }
        return;
    }

    // Get version
    cJSON *version = cJSON_GetObjectItem(root, "version");
    if (cJSON_IsNumber(version)) {
        printk("Version: %d\n", version->valueint);
    } else {
        printk("Version is not a number\n");
    }

	// Get timestamp
	cJSON *timestamp = cJSON_GetObjectItem(root, "timestamp");
	if (cJSON_IsNumber(timestamp)) {
		printk("Timestamp: %d\n", timestamp->valueint);
	} else {
		printk("Timestamp is not a number\n");
	}

	// Empty side items
	for (int i = 0; i < MAX_SIDES; i++) {
		side_items[i] = NULL;
	}
	payload_side_count = 0;

    // Get state
    cJSON *state = cJSON_GetObjectItem(root, "state");
    if (state != NULL) {
        // Iterate over each side config in state
        for (cJSON *side_config = state->child; side_config != NULL; side_config = side_config->next) {
            // Get side number
            int side = atoi(side_config->string);
			printk("Side: %d\n", side);
            // Get id and type
            cJSON *id = cJSON_GetObjectItem(side_config, "id");
            cJSON *type = cJSON_GetObjectItem(side_config, "type");
	

            // Create Settings_data and save it
            Settings_data side_settings_from_json;
            if (id != NULL && cJSON_IsString(id)) {
				
				// Duplicate id string 
				id_str = strdup(id->valuestring);
                side_settings_from_json.id = id_str;
				printk("Id: %s\n", side_settings_from_json.id);

				if (type != NULL && cJSON_IsString(type)) {
                if (strcmp(type->valuestring, "TIME") == 0) {
                    side_settings_from_json.type = "TIME";
                } else if (strcmp(type->valuestring, "COUNT") == 0) {
                    side_settings_from_json.type = "COUNT";
                } 
            	} else {
					side_settings_from_json.type = side_settings[side]->type;
					printk("Copied type from previous: %s\n", side_settings[side]->type);
				}
				printk("Type: %s\n", side_settings_from_json.type);
				save_side_config(side, side_settings_from_json);
				side_items[side] = &side_settings_from_json;
				payload_side_count++;
            } else {
				printk("Throwing away side config due to invalid id or type\n");
			}
		
        }
		
    } else {
        printk("State is not an object\n");
    }
	config_version = version->valueint;
	settings_save_one("config_version", &config_version, sizeof(config_version));;

	// Print json
	char *out = cJSON_Print(root);
	printk("Parsed JSON: %s\n", out);

	// Duplicate contents of state to reported
	cJSON *reported = cJSON_Duplicate(state, 1);
	cJSON *state_reported = cJSON_CreateObject();
	cJSON_AddItemToObject(state_reported, "reported", reported);
	cJSON_ReplaceItemInObject(root, "state", state_reported);

	// Delete metadata and timestamp
	cJSON_DeleteItemFromObject(root, "metadata");
	cJSON_DeleteItemFromObject(root, "timestamp");

	// Print json
	out = cJSON_Print(root);
	printk("Parsed JSON: %s\n", out);

	// Send the modified json to aws without using k_work_reschedule

	struct aws_iot_data tx_data = {
		.qos = MQTT_QOS_0_AT_MOST_ONCE,
		.topic.type = AWS_IOT_SHADOW_TOPIC_UPDATE,
	};

	tx_data.ptr = out;
	tx_data.len = strlen(out);

	LOG_INF("Publishing message: %s to AWS IoT shadow", out);

	int err = aws_iot_send(&tx_data);
	if (err) {
		LOG_ERR("aws_iot_send, error: %d", err);
		FATAL_ERROR();
		return;
	}

	// Free json
	cJSON_free(out);
	cJSON_Delete(root);
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
		LOG_INF("Received message: \"%.*s\" on topic: \"%.*s\"", evt->data.msg.len,
			evt->data.msg.ptr, evt->data.msg.topic.len, evt->data.msg.topic.str);
		if (strncmp(evt->data.msg.topic.str, "$aws/things/T3/shadow/update/delta", evt->data.msg.topic.len) == 0) {
			parse_config_json(evt->data.msg.ptr);
		}
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

static void impact_handler(const struct ext_sensor_evt *const evt)
{
	switch (evt->type) {
			case EXT_SENSOR_EVT_ACCELEROMETER_IMPACT_TRIGGER:
					printf("Impact detected: %6.2f g\n", evt->value);
					// cancel shadow_update, count one, activate led, and rescedule shadow_update with 5 secound delay
					(void)k_work_cancel_delayable(&shadow_update_work);
					occurrence_count ++;
					gpio_pin_set_dt(&led, 1);
					k_work_schedule(&led_off_work, K_SECONDS(0.2));
					(void)k_work_reschedule(&shadow_update_work, K_SECONDS(5));
					
			// Handle other events...
			default:
					break;
	}
}

static int init_led()
{
	int ret;
	// initialize led
	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}
	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return 0;
	}
	k_work_init_delayable(&led_off_work, turn_led_off);
	return ret;
}

static int init_button()
{
	int ret;
	/* init button, when button is pressed call function button-pressed */
	// Initialize impact sensor with a handler function.
	ret = ext_sensors_init(impact_handler);
	if (ret) {
			printf("Error initializing sensors: %d\n", ret);
			return ret;
	}

	// inititilize button with interruption event
	if (!gpio_is_ready_dt(&button)) {
		printk("Error: button device %s is not ready\n",
		       button.port->name);
		return 0;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, button.port->name, button.pin);
		return 0;
	}
	return ret;
}

int main(void)
{	
	/* set side and create new side function to detect change */
	// side = get_side(sensor);

	int ret;
	// initialize led function
	ret = init_led();
	// initialize button function
	ret = init_button();

	int err;
	err = start_settings_subsystem();
	if (err) {
		LOG_ERR("Error starting settings subsystem: %d", err);
		FATAL_ERROR();
		return err;
	}

	// Print all loaded settings
	for (int i = 0; i < MAX_SIDES; i++) {
		printk("Side %d id: %s\n", i, side_settings[i]->id);
		printk("Side %d type: %s\n", i, side_settings[i]->type);
	}

	// start the aws iot sample
	LOG_INF("The AWS IoT sample started, version: %s", CONFIG_AWS_IOT_SAMPLE_APP_VERSION);

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n",
		       ret, button.port->name, button.pin);
		return 0;
	}

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

	Settings_data test_settings_data;
	
	test_settings_data.id = "test242424";
	test_settings_data.type = "TIME";

	save_side_config(7, test_settings_data);


    return 0;
}