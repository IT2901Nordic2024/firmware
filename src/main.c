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

#include <math.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util_macro.h>
#include <zephyr/drivers/sensor.h>

/* button */
#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
/*static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});
static struct gpio_callback button_cb_data;*/

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

/* additional definitions */

/*
 *based on sensor sample
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
/*
 * based on sensor sample
 */

/* Side of the device for sending based on rotation*/
int side;
int newSide;
double pitch;
double roll;

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
	// printf("Value of X: %s\n", accelX);
	// printf("Value of Y: %s\n", accelY);
	// printf("Value of Z: %s\n", accelZ);
	return 0;
}

void calculate_angles(double accelX, double accelY, double accelZ)
{
	double roll = atan2(accelY, accelZ);
	printk("roll value: %f\n", roll);
}

double Xaccel[10] = {};
double Yaccel[10] = {};
double Zaccel[10] = {};

double medianX;
double medianY;
double medianZ;

/*Config for each side*/
// struct to represent the range values for each side

struct range_values {
	double min;
	double max;
};

struct each_side {
	struct range_values accelX;
	struct range_values accelY;
	struct range_values accelZ;
};

/*endre til normal vectors len 12*/
struct each_side sides[6] = {
	{{-2.2025, 2.2025}, {-2.2025, 2.2025}, {-12.0125, -7.6075}}, // side 1
	{{-2.2025, 2.2025}, {7.6075, 12.0125}, {-2.2025, 2.2025}},   // side 2
	{{7.6075, 12.0125}, {-2.2025, 2.2025}, {-2.2025, 2.2025}},   // side 3
	{{-12.0125, -7.6075}, {-2.2025, 2.2025}, {-2.2025, 2.2025}}, // side 4
	{{-2.2025, 2.2025}, {-12.0125, -7.6075}, {-2.2025, 2.2025}}, // side 5
	{{-2.2025, 2.2025}, {-2.2025, 2.2025}, {7.6075, 12.0125}},   // side 6
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

// Function to calculate median
double calculate_median(double accel[], int array_size)
{
	/*Sort list of values*/
	qsort(accel, array_size, sizeof(int), compare);

	/*Check if array size even or odd to know whitch median teqnique to use */
	if (array_size % 2 == 0) {
		return (accel[array_size / 2 - 1] + accel[array_size / 2]) / 2.0;
	} else {
		return accel[array_size / 2];
	}
}

/*parametere er to lister på len 3*/
double vector_dot_product(double vector1[], double vector2[])
{
	return vector1[0] * vector2[0] + vector1[1] * vector2[1] + vector1[2] * vector2[2];
}

int find_what_side(struct each_side sides[], int number_of_sides)
{
	/*liste av enhetsvektorer ikke side verdier*/
	double sidearray[3] = {1, 2, 3};
	double nowarray[3] = {medianX, medianY, medianZ};
	/*endre delta*/
	double delta = 2;
	for (size_t i = 0; i < number_of_sides; i++) {
		struct each_side side = sides[i];

		/*normal vektoren for siden i stedenfor side*/
		double normal_acc = vector_dot_product(sidearray, nowarray);

		if (normal_acc > 9.81 - delta) {
			return i + 1;
		}
	}

	return -1; // error if valus are not in range
}
void sampling_filter(int number_of_samples, const struct device *dev, int32_t ms)
{
	int ret;

	int count = 0;

	while (count < number_of_samples) {
		ret = sensor_sample_fetch(dev);
		if (ret < 0) {
			printk("sensor_sample_fetch() failed: %d\n", ret);
			// return ret;
		}
		for (size_t i = 0; i < ARRAY_SIZE(channels); i++) {
			ret = sensor_channel_get(dev, channels[i], &accel[i]);
			if (ret < 0) {
				printk("sensor_channel_get(%c) failed: %d\n", 'X' + i, ret);
				// return ret;
			}
		}
		/*double G = sqrt(pow(sensor_value_to_double(&accel[0]), 2) +
				pow(sensor_value_to_double(&accel[1]), 2) +
				pow(sensor_value_to_double(&accel[2]), 2));*/
		Xaccel[count] = sensor_value_to_double(&accel[0]);
		Yaccel[count] = sensor_value_to_double(&accel[1]);
		Zaccel[count] = sensor_value_to_double(&accel[2]);

		/*printk("(%12.6f, %12.6f, %12.6f)\n", sensor_value_to_double(&accel[0]),
		       sensor_value_to_double(&accel[1]), sensor_value_to_double(&accel[2]));
		*/
		count++;
		k_msleep(ms);
	}

	medianX = calculate_median(Xaccel, number_of_samples);
	medianY = calculate_median(Yaccel, number_of_samples);
	medianZ = calculate_median(Zaccel, number_of_samples);

	printk("x: %f y: %f z: %f\n", medianX, medianY, medianZ);
	// printk("Median Y: %f \n", medianY);
	// printk("Median Z: %f \n", medianZ);

	double squaresum = sqrt(pow(medianX, 2) + pow(medianY, 2) + pow(medianZ, 2));
	// printk("Sqr: %f \n", squaresum);
}
int correct_side = 0;
static int get_side(const struct device *dev)
{
	/*
	 * Fetch sensor data from the accelerometer sensor
	 * and return the side of the device based on the z-axis
	 * 1 for up and -1 for down
	 */
	// int ret;
	//	Check if device is ready, if not return 0
	if (!device_is_ready(dev)) {
		printk("sensor: device not ready.\n");
		return 0;
	}

	/*bruk 500-600 samples for å finne vektor*/
	sampling_filter(10, dev, 100);

	/*correct_side = find_what_side(sides, 6);*/

	/*printk("Side: %i", correct_side);*/
	return 0;
	// int count = 0;
	// double M_PI = 3.14;

	// double squaresum = sqrt(pow(medianX, 2) + pow(medianY, 2) + pow(medianZ, 2));

	// pitch = asin(medianX / squaresum) * 180 / M_PI;

	// roll = atan2(medianY, medianZ) * 180 / M_PI;

	// printk("Pitch: %f \nRoll: %f \n", pitch, roll);
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
	fetch_accels(sensor);
	struct payload payload = {
		.state.reported.uptime = k_uptime_get(),
		.state.reported.accelX = accelX,
		.state.reported.accelY = accelY,
		.state.reported.accelZ = accelZ,
		.state.reported.correct_side = correct_side,
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

	(void)k_work_reschedule(&shadow_update_work,
				K_SECONDS(CONFIG_AWS_IOT_SAMPLE_PUBLICATION_INTERVAL_SECONDS));
}

/* System Workqueue handlers. */
/* put shadow update work infornt of the kwork queue that sends event to aws */
void event_trigger()
{
	printk("event_trigger\n");
	/* send shadow_update_work in front of the queue */
	(void)k_work_reschedule(&shadow_update_work, K_NO_WAIT);
}

/* function with a while loop that checks if side is changed */
static void check_position(void)
{
	while (true) {

		newSide = get_side(sensor);
		/*if side is changed send event trigger*/
		if (side != 0 && side != newSide) {
			side = newSide;
			event_trigger();
		}
		/*endre når finne vektor fordi antall samples øker*/
		k_msleep(1000);
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

#if defined(CONFIG_BOOTLOADER_MCUBOOT)
	boot_write_img_confirmed();
#endif

	/* Start sequential updates to AWS IoT. */
	(void)k_work_reschedule(&shadow_update_work, K_NO_WAIT);

	/* set button pressed as buttons funcion
	gpio_init_callback(&button_cb_data, event_trigger, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
	printk("Set up button at %s pin %d\n", button.port->name, button.pin); */

	/* Start to check the position */
	check_position();
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

int main(void)
{
	/* set side and create new side function to detect change */
	/*side = get_side(sensor);*/

	/* init button, when button is pressed call function button-pressed */
	// int ret;
	/*
		if (!gpio_is_ready_dt(&button)) {
			printk("Error: button device %s is not ready\n", button.port->name);
			return 0;
		}*/

	LOG_INF("The AWS IoT sample started, version: %s", CONFIG_AWS_IOT_SAMPLE_APP_VERSION);

	/*
		ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
		if (ret != 0) {
			printk("Error %d: failed to configure %s pin %d\n", ret, button.port->name,
			       button.pin);
			return 0;
		}*/
	/*
		ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
		if (ret != 0) {
			printk("Error %d: failed to configure interrupt on %s pin %d\n", ret,
			       button.port->name, button.pin);
			return 0;
		}
	*/
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

	return 0;
}
