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

static struct nvs_fs fs = {
    .flash_device = NVS_FLASH_DEVICE,
    .sector_size = NVS_SECTOR_SIZE,
    .sector_count = NVS_SECTOR_COUNT,
    .offset = NVS_STORAGE_OFFSET,
};

// SETTINGS SUBSYSTEM
#define DEFAULT_TYPE_VALUE 0

struct settings_data {
    int32_t timestamp;
    int32_t id;
    int32_t type;
};

static struct settings_data side_0_settings = { .timestamp = 0, .id = 0, .type = DEFAULT_TYPE_VALUE };

static int config_settings_set(const char *name, size_t len,
                                settings_read_cb read_cb, void *cb_arg)
{
    const char *next;
    int rc;

    // Check if the settings name matches and no extra characters after "type"
    if (settings_name_steq(name, "type", &next) && !next) {
        // Check the length
        if (len != sizeof(side_0_settings.type)) {
            return -EINVAL;
        }

        // Read the type
        rc = read_cb(cb_arg, &side_0_settings.type, sizeof(side_0_settings.type));
        if (rc >= 0) {
            return 0;
        }
        return rc;
    }
    else if (settings_name_steq(name, "timestamp", &next) && !next) {
        // Check the length
        if (len != sizeof(side_0_settings.timestamp)) {
            return -EINVAL;
        }

        // Read the timestamp
        rc = read_cb(cb_arg, &side_0_settings.timestamp, sizeof(side_0_settings.timestamp));
        if (rc >= 0) {
            return 0;
        }
        return rc;
    }
    else if (settings_name_steq(name, "id", &next) && !next) {
        // Check the length
        if (len != sizeof(side_0_settings.id)) {
            return -EINVAL;
        }

        // Read the id
        rc = read_cb(cb_arg, &side_0_settings.id, sizeof(side_0_settings.id));
        if (rc >= 0) {
            return 0;
        }
        return rc;
    }

    return -ENOENT;
}

struct settings_handler side_0_conf = {
    .name = "side_0",
    .h_set = config_settings_set
};


int main(void)
{
    settings_subsys_init();
    settings_register(&side_0_conf);
    settings_load();
    
    // Print loaded settings
    printk("Loaded side_0/timestamp: %" PRIi32 "\n", side_0_settings.timestamp);
    printk("Loaded side_0/id: %" PRIi32 "\n", side_0_settings.id);
    printk("Loaded side_0/type: %" PRIi32 "\n", side_0_settings.type);
    
    // Increment type by 1 and save settings
	side_0_settings.id = side_0_settings.id + 1242;
	settings_save_one("side_0/id", &side_0_settings.id, sizeof(side_0_settings.id));
	side_0_settings.timestamp = side_0_settings.timestamp + 1000;
	settings_save_one("side_0/timestamp", &side_0_settings.timestamp, sizeof(side_0_settings.timestamp));
    side_0_settings.type = side_0_settings.type + 1;
    settings_save_one("side_0/type", &side_0_settings.type, sizeof(side_0_settings.type));
    
    // Print saved settings
    printk("Saved side_0/type: %" PRIi32 "\n", side_0_settings.type);
    
    // Wait for 1000 milliseconds and reboot
    k_msleep(1000);
    sys_reboot(SYS_REBOOT_COLD);
    
    return 0;
}