# nRFHabitTracker - Firmware

This repository supports the Thingy:91 from Nordic Semiconductor and is intented to be used in conjunction with the front end and back end [repositories found here](https://github.com/IT2901Nordic2024).

The sample is developed based on the [AWS IoT sample](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/samples/net/aws_iot/README.html) developed by Nordic Semiconductor, but extends the functionality by integrating sensors and persistent storage by integrating an FCB backend utilising the [Zephyr Settings subsystem](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/zephyr/services/settings/index.html).

The firmware exclusively supports the [Thingy:91](https://www.nordicsemi.com/Products/Development-hardware/Nordic-Thingy-91) board and the board should be enclosed in a 3D printed dodecahedron [as shown here](https://www.youtube.com/watch?v=Rflk_TW2EQo&feature=youtu.be) for the orientation tracking to work.

## Cloning the project

To acces the project locally, you need to clone the project. This can be done by the command: `git clone https://github.com/IT2901Nordic2024/firmware.git`

## Preparing AWS

Before flashing this sample you should follow the steps outlined in the [setup and configuration section of the AWS IoT library](https://www.youtube.com/watch?v=Rflk_TW2EQo&feature=youtu.be). For the 5th step, the `CONFIG_AWS_IOT_BROKER_HOST_NAME`, `CONFIG_AWS_IOT_CLIENT_ID_STATIC` and `CONFIG_MQTT_HELPER_SEC_TAG` should all [be set in prj.conf](prj.conf) before flashing the sample to your board.

## Creating a build configuration with NRF connect

By using the NRF connect vs-code extension: `https://marketplace.visualstudio.com/items?itemName=nordic-semiconductor.nrf-connect` you can create a new build configuration.
These are the settings you have to select to ensure the build configuration is correct.

- **Board:** _thingy91_nrf9160_ns_
- **Configuration:** _prj.conf_
- **Kconfig fragments:** _boards/thingy91_nrf9160_ns.config_
- **Devicetree overlay:** _boards/thingy91_nrf9160_ns.overlay_

## Flashing the device

After building the firmware we recommend you use the `Erase and flash to device` function when flashing a new build, especially if you have already run this firmware previously. This is because the Settings subsystem can create undefined behaviour when the storage is not erased before flashing.

## Files and structure

The sample consists of two main parts, the AWS IoT communication handlers and the side orientation handlers. Most of the functionality lies within [main.c](src/main.c) where the accelerometer is polled every 100 milliseconds and each second the median of the last 10 accelerometer values is used to find which side is currently oriented upwards. If the current side has a habit stored it will load its type and either enable the counter or begin time tracking depending on what type of habit it is.

The [settings_defs folder](src/settings_defs/) describe how habits are stored using the Settings subsystem.
The [ext_sensors folder](ext_sensors/) is imported from [Asset Tracker v2](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/applications/asset_tracker_v2/README.html) and is currently not in use as we poll the accelerometer directly, but it makes it easier to implement new habit types if desired.

## Using the sample

When using the sample, the device will first connect to the AWS IoT broker host and subscribe to the device's delta topic to find the desired state from the back end. If there is a delta it will then parse the delta message and save each side to its local configuration and respond to AWS with the reported changes to clear the delta state.

When a user orients the device with a habit side up it will then either enable the counter if the habit is of type COUNT or begin tracking time if the habit is of type TIME. When time tracking it will continue tracking the time until the device is reoriented at which point it will send the start and stop timestamp to AWS using Protocol Buffers.

When counting the user has to initiate a high G impact by smacking the device on a surface such as a table which increments the count. If the count has not been incremented further within 5 seconds the current count is sent using Protobuf and the counter is reset to 0. The counter system is enabled until the device is oriented to a new side.
