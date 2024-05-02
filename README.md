# Firmware repository for IT2901 - Informatics project II

Currently significantly based off of AWS IoT sample from Nordic. Replace AWS Broker Host name, AWS IoT security tag and client ID as needed.

## Cloning the project

To get acces to the project locally, you need to clone the project. This can be done by the command: `git clone https://github.com/IT2901Nordic2024/firmware.git`

## Specify config

You need to specify what device you are flashing to, this is done in the prj.conf file: CONFIG_AWS_IOT_CLIENT_ID_STATIC="your-device" where your-device is your device.

## Creating a build with NRF connect

By using the NRF connect vs-code extension: `https://marketplace.visualstudio.com/items?itemName=nordic-semiconductor.nrf-connect` you can create a new build configuration. 

- __Board:__ _thingy91_nrf9160_ns_
- __Configuration:__ _prj.conf_
- __Kconfig fragments:__ _boards/thingy91_nrf9160_ns.config_
- __Devicetree overlay:__ _boards/thingy91_nrf9160_ns.overlay_

## Flashing to device

After connecting the thingy91 and nrf9160 devkit By using the NRF connect extension you can select and flash your build to your device.

