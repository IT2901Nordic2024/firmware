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

static struct nvs_fs fs = {
    .flash_device = NVS_FLASH_DEVICE,
    .sector_size = NVS_SECTOR_SIZE,
    .sector_count = NVS_SECTOR_COUNT,
    .offset = NVS_STORAGE_OFFSET,
};


int main(void)
{
    settings_subsys_init();
    for (int i = 0; i < MAX_SIDES; i++) {
		settings_register(side_confs[i]);
	}
    settings_load();
    
    // Print loaded settings
    for (int i = 0; i < MAX_SIDES; i++) {
		printk("Loaded side_%d/timestamp: %" PRIi32 "\n", i, side_settings[i]->timestamp);
		printk("Loaded side_%d/id: %" PRIi32 "\n", i, side_settings[i]->id);
		printk("Loaded side_%d/type: %" PRIi32 "\n", i, side_settings[i]->type);
	}

    
	for (int i = 0; i < MAX_SIDES; i++) {
		struct settings_data *side = side_settings[i];
		side_settings[i]->timestamp = side->timestamp + 1000;
		side_settings[i]->id = side->id + 123;
		side_settings[i]->type = side->type + 1;
		
		char name[20];
		sprintf(name, "side_%d/timestamp", i);

		int ret = settings_save_one(name, &side->timestamp, sizeof(side->timestamp));
		if (ret) {
			printk("Error saving side_%d/timestamp: %d\n", i, ret);
		} else {
			printk("Saved side_%d/timestamp: %" PRIi32 "\n", i, side->timestamp);
		}

		sprintf(name, "side_%d/id", i);
		ret = settings_save_one(name, &side->id, sizeof(side->id));

		if (ret) {
			printk("Error saving side_%d/id: %d\n", i, ret);
		} else {
			printk("Saved side_%d/id: %" PRIi32 "\n", i, side->id);
		}

		sprintf(name, "side_%d/type", i);
		ret = settings_save_one(name, &side->type, sizeof(side->type));
		if (ret) {
			printk("Error saving side_%d/type: %d\n", i, ret);
		} else {
			printk("Saved side_%d/type: %" PRIi32 "\n", i, side->type);
		}
	}
		

    
    
    // Wait for 1000 milliseconds and reboot
    k_msleep(5000);
    sys_reboot(SYS_REBOOT_COLD);
    
    return 0;
}