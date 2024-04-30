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
#include <ext_sensors.h>

#include <date_time.h>
#include <zephyr/drivers/pwm.h>

/*Settings and NVS*/
#include <zephyr/settings/settings.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/device.h>
#include <string.h>
#include <inttypes.h>
#include <zephyr/sys/printk.h>
#include "../ext_sensors/ext_sensors.h"

/*Protobuf*/
#include <pb.h>
#include <pb_common.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include <src/data.pb.h>

/* button */
#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});
static struct gpio_callback button_cb_data;
struct pwm_dt_spec sBuzzer = PWM_DT_SPEC_GET(DT_ALIAS(buzzer_pwn));

typedef struct settings_data Settings_data;

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

/* additional definitions */
// define topic based on the thing
#define MY_CUSTOM_TOPIC "habit-tracker-data/T3/events"

static struct aws_iot_topic_data myTopic = {
	.str = MY_CUSTOM_TOPIC,
	.len = strlen(MY_CUSTOM_TOPIC),
};

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
int32_t occurrence_count = 0;
bool counter_active = false;

/* time variables */
int64_t unix_time;
int64_t start_time;

/* LED */
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

/* Side of the device for sending based on rotation */
int side = 0;
int newSide;
int correct_side;

// temporarly values for sides. "" is default, "COUNT" is count, "TIMER" is timer
char *side_0[] = {"COUNT", "1714396295426"};
char *side_1[] = {"TIME", "1714396736027"};
char *side_2[] = {"COUNT", "1714402198197"};
char *side_3[] = {"COUNT", "1714402288037"};
char *side_4[] = {"COUNT", "1714402306888"};
char *side_5[] = {"TIME", "1714402317862"};
char *side_6[] = {"TIME", "1714402326610"};
char *side_7[] = {"TIME", "1714402338234"};
char *side_8[] = {"", ""};
char *side_9[] = {"", ""};
char *side_10[] = {"", ""};

/* Zephyr NET management event callback structures. */
static struct net_mgmt_event_callback l4_cb;
static struct net_mgmt_event_callback conn_cb;

/* Forward declarations. */
static void shadow_update_work_fn(struct k_work *work);
static void connect_work_fn(struct k_work *work);
static void aws_iot_event_handler(const struct aws_iot_evt *const evt);
static void counter_stop_fn(struct k_work *work);
static void set_newSide_fn(struct k_work *work);
static void start_timer_fn(struct k_work *work);
static void stop_timer_fn(struct k_work *work);
static void turn_led_off(struct k_work *work);
static void check_position();
static void create_message();

/* Work items used to control some aspects of the sample. */
static K_WORK_DELAYABLE_DEFINE(shadow_update_work, shadow_update_work_fn);
static K_WORK_DELAYABLE_DEFINE(connect_work, connect_work_fn);
static K_WORK_DELAYABLE_DEFINE(led_off_work, turn_led_off);
static K_WORK_DELAYABLE_DEFINE(counter_stop, counter_stop_fn);
static K_WORK_DELAYABLE_DEFINE(set_newSide, set_newSide_fn);
static K_WORK_DELAYABLE_DEFINE(start_timer, start_timer_fn);
static K_WORK_DELAYABLE_DEFINE(stop_timer, stop_timer_fn);

/* Create thread for checking the position of the device */
K_THREAD_STACK_DEFINE(stack_area, 2048);
struct k_thread check_pos_data;

/* mock data */
// returns the value of the side based on the side number
static char *get_side_value(int side)
{
	switch (side) {
	case 1:
		return side_0[0];
	case 2:
		return side_1[0];
	case 3:
		return side_2[0];
	case 4:
		return side_3[0];
	case 5:
		return side_4[0];
	case 6:
		return side_5[0];
	case 7:
		return side_6[0];
	case 8:
		return side_7[0];
	case 9:
		return side_8[0];
	case 10:
		return side_9[0];
	case 11:
		return side_10[0];
	default:
		return "";
	}
}
// returns the habit based on side
static char *get_habit_id(int side)
{
	switch (side) {
	case 1:
		return side_0[1];
	case 2:
		return side_1[1];
	case 3:
		return side_2[1];
	case 4:
		return side_3[1];
	case 5:
		return side_4[1];
	case 6:
		return side_5[1];
	case 7:
		return side_6[1];
	case 8:
		return side_7[1];
	case 9:
		return side_8[1];
	case 10:
		return side_9[1];
	case 11:
		return side_10[1];
	default:
		return "";
	}
}

/* Static functions */
static int32_t int64_to_int32(int64_t large_value)
{
	// scale down int64 to int32
	return (int32_t)(large_value / 1000);
}

static bool encode_string(pb_ostream_t *stream, const pb_field_t *field, void *const *arg)
{
	// encode string so protobuff accept it
	const char *str = (const char *)(*arg);
	if (!pb_encode_tag_for_field(stream, field)) {
		return false;
	}
	return pb_encode_string(stream, (uint8_t *)str, strlen(str));
}

static void turn_led_off(struct k_work *work)
{
	// turn led off in a work function
	gpio_pin_set_dt(&led, 0);
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
	} else {
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

	return 0;
}

double Xaccel[10] = {};
double Yaccel[10] = {};
double Zaccel[10] = {};

double medianX;
double medianY;
double medianZ;

/*Config for each side*/

struct each_side {
	double accelX;
	double accelY;
	double accelZ;
};

/*Normal vectors for each side*/
struct each_side normal_vectors[12] = {
	{-0.003696148, -0.069007951, -0.996817534}, // side 0
	{0.879237385, -0.308156155, -0.361714449},  // side 1
	{-0.024281719, -0.90438758, -0.422177015},  // side 2
	{-0.884967095, -0.232578156, -0.400957651}, // side 3
	{-0.553576905, 0.735285768, -0.389626839},  // side 4
	{0.525613609, 0.741611336, -0.415332151},   // side 5
	{-0.541858156, -0.691422294, 0.476691277},  // side 6
	{0.475564405, -0.757096215, 0.442519528},   // side 7
	{0.810239622, 0.211181093, 0.540259636},    // side 8
	{-0.031841904, 0.852591043, 0.519405125},   // side 9
	{-0.857683398, 0.311294211, 0.408629604},   // side 10
	{0.005361934, -0.033163197, 0.995770246},   // side 11
};

int compare(const void *a, const void *b)
{
	double *double_a = (double *)a;
	double *double_b = (double *)b;
	// Compare the doubles
	if (&double_a < &double_b) {
		return -1; // Return a negative value if a should appear before b
	} else if (&double_a > &double_b) {
		return 1; // Return a positive value if a should appear after b
	} else {
		return 0; // Return 0 if a and b are equal
	}
}

/* Function to calculate median*/
double calculate_median(double accel[], int array_size)
{
	// sort list
	qsort(accel, array_size, sizeof(int), compare);

	// find median
	if (array_size % 2 == 0) {
		return (accel[array_size / 2 - 1] + accel[array_size / 2]) / 2.0;
	} else {
		return accel[array_size / 2];
	}
}

/*Calculate dot product of two vectors*/
double vector_dot_product(double vector1[], double vector2[])
{
	return vector1[0] * vector2[0] + vector1[1] * vector2[1] + vector1[2] * vector2[2];
}

/*Returns what side is up on the dodd*/
int find_what_side(struct each_side sides[], int number_of_sides)
{

	double median_vector[3] = {medianX, medianY, medianZ};

	double delta = 2;

	for (size_t i = 0; i < number_of_sides; i++) {

		double normal_vector[3] = {sides[i].accelX, sides[i].accelY, sides[i].accelZ};

		double normal_acc = vector_dot_product(normal_vector, median_vector);

		if (normal_acc > 9.81 - delta) {
			return i;
		}
	}

	return -1; // error if valus are not in range
}

/*help values to store double values to send to aws*/
char median_values_X[10];
char median_values_Y[10];
char median_values_Z[10];

/*Use a median filter to remove noise from accel values*/
void sampling_filter(int number_of_samples, const struct device *dev, int32_t ms)
{
	int ret;

	int count = 0;

	while (count < number_of_samples) {
		ret = sensor_sample_fetch(dev);
		if (ret < 0) {
			printk("sensor_sample_fetch() failed: %d\n", ret);
		}
		for (size_t i = 0; i < ARRAY_SIZE(channels); i++) {
			ret = sensor_channel_get(dev, channels[i], &accel[i]);
			if (ret < 0) {
				printk("sensor_channel_get(%c) failed: %d\n", 'X' + i, ret);
			}
		}

		Xaccel[count] = sensor_value_to_double(&accel[0]);
		Yaccel[count] = sensor_value_to_double(&accel[1]);
		Zaccel[count] = sensor_value_to_double(&accel[2]);

		count++;
		k_msleep(ms);
	}

	medianX = calculate_median(Xaccel, number_of_samples);
	medianY = calculate_median(Yaccel, number_of_samples);
	medianZ = calculate_median(Zaccel, number_of_samples);

	snprintf(median_values_X, sizeof(median_values_X), "%f", medianX);
	snprintf(median_values_Y, sizeof(median_values_Y), "%f", medianY);
	snprintf(median_values_Z, sizeof(median_values_Z), "%f", medianZ);
}

static int get_side(const struct device *dev)
{
	//	Check if device is ready, if not return 0
	if (!device_is_ready(dev)) {
		printk("sensor: device not ready.\n");
		return 0;
	}
	// apply median filter to accel values
	sampling_filter(10, dev, 100);
	// find what side is up
	correct_side = find_what_side(normal_vectors, 12);
	// printk("Side: %i \n", correct_side);
	return correct_side;
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
	char message[CONFIG_AWS_IOT_SAMPLE_JSON_MESSAGE_SIZE_MAX] = {0};
	/* Fetch and send sensor data */
	// fetch_accels(sensor);
	struct payload payload = {
		.state.reported.uptime = k_uptime_get(),
	};

	occurrence_count = 0;

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
// static void event_trigger()
// {
// 	printk("event_trigger\n");
// 	/* send shadow_update_work in front of the queue */
// 	(void)k_work_reschedule(&shadow_update_work, K_NO_WAIT);
// }

static void counter_stop_fn(struct k_work *work)
{
	habit_data message = habit_data_init_zero;
	date_time_now(&unix_time);
	message.device_timestamp = int64_to_int32(unix_time);
	message.data = occurrence_count;
	message.habit_id.arg = get_habit_id(side);
	message.habit_id.funcs.encode = &encode_string;
	occurrence_count = 0;
	create_message(message);
}

static void impact_handler(const struct ext_sensor_evt *const evt)
{
	switch (evt->type) {
	case EXT_SENSOR_EVT_ACCELEROMETER_IMPACT_TRIGGER:
		// if counter is active run the impact handler
		if (counter_active) {
			printf("Impact detected: %6.2f g\n", evt->value);
			// cancel, count one, activate led, and rescedule with 5 secound delay
			k_work_cancel_delayable(&counter_stop);
			occurrence_count++;
			gpio_pin_set_dt(&led, 1);
			k_work_schedule(&led_off_work, K_SECONDS(0.2));
			k_work_reschedule(&counter_stop, K_SECONDS(5));
		}
	default:
		break;
	}
}

static void start_timer_fn(struct k_work *work)
{
	// start_time and send event trigger
	int ret;
	ret = date_time_now(&unix_time);
	// checks if time is updated else try again
	if (ret == 0) {
		printk("Starting timer\n");
		start_time = unix_time;
		printk("Sending message:\n");
		gpio_pin_set_dt(&led, 1);
	} else {
		LOG_ERR("Error getting time");
	}
}

static void stop_timer_fn(struct k_work *work)
{
	// stop_time and send event trigger
	int ret;
	// checks if time is updated else try again
	if (ret == 0) {
		printk("Stopping timer\n");
		habit_data message = habit_data_init_zero;
		message.device_timestamp = int64_to_int32(unix_time);
		message.habit_id.arg = get_habit_id(side);
		message.habit_id.funcs.encode = &encode_string;
		message.start_timestamp = int64_to_int32(start_time);
		message.stop_timestamp = int64_to_int32(unix_time);
		create_message(message);
		k_work_schedule(&led_off_work, K_NO_WAIT);
	} else {
		LOG_ERR("Error getting time");
	}
}

static void set_newSide_fn(struct k_work *work)
{
	side = newSide;
	// if the new side is count start the count
	if (strcmp(get_side_value(side), "COUNT") == 0) {
		counter_active = true;
	}
	// if the new side is time start the timer
	if (strcmp(get_side_value(side), "TIME") == 0) {
		printk("Starting timerrrrr\n");
		k_work_reschedule(&start_timer, K_NO_WAIT);
	}
}

/* function to check side, runs in separate tread */
static void check_position()
{
	while (true) {
		newSide = get_side(sensor);
		// if side is changed and the new side is not -1
		if (newSide != -1 && side != newSide) {
			// if the prew side is count stop the count
			if (strcmp(get_side_value(side), "COUNT") == 0) {
				counter_active = false;
				k_work_reschedule(&counter_stop, K_NO_WAIT);
			}
			// if the prew side is time stop the timer
			if (strcmp(get_side_value(side), "TIME") == 0) {
				k_work_reschedule(&stop_timer, K_NO_WAIT);
			}
			// set the new side
			k_work_reschedule(&set_newSide, K_NO_WAIT);
		}
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
}

static void on_aws_iot_evt_disconnected(void)
{
	/* Remove button callback
	gpio_remove_callback(button.port, &button_cb_data);
	printk("Button at %s pin %d has been deactivated\n", button.port->name, button.pin); */
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

static void save_side_config(int side, Settings_data side_settings)
{
	char name[20];
	sprintf(name, "side_%d/timestamp", side);
	int ret =
		settings_save_one(name, &side_settings.timestamp, sizeof(side_settings.timestamp));
	if (ret) {
		printk("Error saving side_%d/timestamp: %d\n", side, ret);
	}

	sprintf(name, "side_%d/id", side);
	ret = settings_save_one(name, &side_settings.id, sizeof(side_settings.id));

	if (ret) {
		printk("Error saving side_%d/id: %d\n", side, ret);
	}

	sprintf(name, "side_%d/type", side);
	ret = settings_save_one(name, &side_settings.type, sizeof(side_settings.type));
	if (ret) {
		printk("Error saving side_%d/type: %d\n", side, ret);
	}
}

static int start_settings_subsystem()
{
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
	err = settings_load();
	if (err) {
		printk("Error loading settings: %d\n", err);
		return err;
	}
	return 0;
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
		/* on iot ready create a new thred for start to check the position */
		k_thread_create(&check_pos_data, stack_area, K_THREAD_STACK_SIZEOF(stack_area),
				check_position, NULL, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO,
				0, K_NO_WAIT);
		/* set button pressed as buttons funcion */
		gpio_init_callback(&button_cb_data, create_message, BIT(button.pin));
		gpio_add_callback(button.port, &button_cb_data);
		printk("Set up button at %s pin %d\n", button.port->name, button.pin);
		break;
	case AWS_IOT_EVT_DISCONNECTED:
		LOG_INF("AWS_IOT_EVT_DISCONNECTED");
		on_aws_iot_evt_disconnected();
		break;
	case AWS_IOT_EVT_DATA_RECEIVED:
		LOG_INF("AWS_IOT_EVT_DATA_RECEIVED");
		// save_config(evt->data.msg.topic.str, sizeof(evt->data.msg.topic.str));
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
	return ret;
	k_work_schedule(&led_off_work, K_NO_WAIT);
}

static int init_button()
{
	int ret;
	/* init button, when button is pressed call function button-pressed */
	// inititilize button with interruption event
	if (!gpio_is_ready_dt(&button)) {
		printk("Error: button device %s is not ready\n", button.port->name);
		return 0;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n", ret,
		       button.port->name, button.pin);
		return 0;
	}
	return ret;
}

static void create_message(habit_data message)
{
	// Create a buffer to hold the serialized data
	uint8_t buffer[128];
	pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

	// Encode the message
	bool status = pb_encode(&stream, habit_data_fields, &message);
	if (!status) {
		printf("Encoding failed: %s\n", PB_GET_ERROR(&stream));
		return;
	}

	printf("send protobuff message \n");
	struct aws_iot_data data = {
		.qos = MQTT_QOS_0_AT_MOST_ONCE,
		.topic = myTopic,
		.ptr = buffer,
		.len = stream.bytes_written,
	};

	// LOG_INF("Publishing message: %s to AWS IoT shadow", data.ptr);
	int err = aws_iot_send(&data);
	if (err) {
		LOG_ERR("aws_iot_send, error: %d", err);
		FATAL_ERROR();
		return;
	}
	return;
}

int play_tone(int frequency, int duration)
{
	if (!pwm_is_ready_dt(&sBuzzer)) {
		printk("Error: PWM device %s is not ready\n",
		       sBuzzer.dev->name);
		return -1;
	}
	uint32_t pwm_period_ns = NSEC_PER_SEC / frequency;
	uint32_t pwm_duty_cycle_ns = pwm_period_ns / 2;
	// Set the PWM period and duty cycle
	if (pwm_set_dt(&sBuzzer, pwm_period_ns, pwm_duty_cycle_ns)) {
        printk("Error: Failed to set PWM period and duty cycle\n");
        return -2;
    }

	// Play the tone for the specified duration
	k_sleep(K_MSEC(duration));

	// Turn off the PWM signal
	if (pwm_set_dt(&sBuzzer, 0, 0)) {
        printk("Error: Failed to turn off note\n");
        return -3;
    }

	return 0;
}

void boot_sound() {
    play_tone(264, 250);
    k_sleep(K_MSEC(500));
    play_tone(264, 250);
    k_sleep(K_MSEC(250));
    play_tone(297, 1000);
    k_sleep(K_MSEC(250));
    play_tone(264, 1000);
    k_sleep(K_MSEC(250));
    play_tone(352, 1000);
    k_sleep(K_MSEC(250));
    play_tone(330, 2000);
    k_sleep(K_MSEC(500));
    play_tone(264, 250);
    k_sleep(K_MSEC(500));
    play_tone(264, 250);
    k_sleep(K_MSEC(250));
    play_tone(297, 1000);
    k_sleep(K_MSEC(250));
    play_tone(264, 1000);
    k_sleep(K_MSEC(250));
    play_tone(396, 1000);
    k_sleep(K_MSEC(250));
    play_tone(352, 2000);
    k_sleep(K_MSEC(500));
    play_tone(264, 250);
    k_sleep(K_MSEC(500));
    play_tone(264, 250);
    k_sleep(K_MSEC(250));
    play_tone(264, 1000);
    k_sleep(K_MSEC(250));
    play_tone(440, 1000);
    k_sleep(K_MSEC(250));
    play_tone(352, 500);
    k_sleep(K_MSEC(250));
    play_tone(352, 250);
    k_sleep(K_MSEC(250));
    play_tone(330, 1000);
    k_sleep(K_MSEC(250));
    play_tone(297, 2000);
    k_sleep(K_MSEC(500));
    play_tone(466, 250);
    k_sleep(K_MSEC(500));
    play_tone(466, 250);
    k_sleep(K_MSEC(250));
    play_tone(440, 1000);
    k_sleep(K_MSEC(250));
    play_tone(352, 1000);
    k_sleep(K_MSEC(250));
    play_tone(396, 1000);
    k_sleep(K_MSEC(250));
    play_tone(352, 2000);
    k_sleep(K_MSEC(250));
}

int alternative_boot_sound(){
	int ret;
	ret = play_tone(1760, 100);
	
	ret = play_tone(2637, 500);

	k_sleep(K_MSEC(1000));
	return ret;
}




int main(void)
{
	int ret;

	boot_sound();

	// initialize led function
	ret = init_led();
	// initialize button function
	ret = init_button();

	ret = ext_sensors_init(impact_handler);
	if (ret) {
		printf("Error initializing sensors: %d\n", ret);
		return ret;
	}

	int err;
	err = start_settings_subsystem();
	if (err) {
		LOG_ERR("Error starting settings subsystem: %d", err);
		FATAL_ERROR();
		return err;
	}

	// start the aws iot sample
	LOG_INF("The AWS IoT sample started, version: %s", CONFIG_AWS_IOT_SAMPLE_APP_VERSION);

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n", ret, button.port->name,
		       button.pin);
		return 0;
	}
	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n", ret,
		       button.port->name, button.pin);
		return 0;
	}

	/* init the aws connection */
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
	return 0;
}