#
# Copyright (c) 2020 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(aws_iot)

# NORDIC SDK APP START
target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/json_payload/json_payload.c)
target_sources(app PRIVATE ext_sensors/ext_sensors.c)
target_sources(app PRIVATE src/proto/config.pb.cc)
# NORDIC SDK APP END

zephyr_include_directories(src)
zephyr_include_directories(src/json_payload)
zephyr_include_directories(ext_sensors)
zephyr_include_directories(src/proto)

# Include generated nanopb files
zephyr_include_directories(${Protobuf_INCLUDE_DIRS})
nanopb_generate_cpp(proto_sources proto_headers RELPATH . src/proto/config.proto)

# Include generated header files
zephyr_library_include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Make folder containing certificates global so that it can be located by
# the AWS IoT library.
zephyr_include_directories_ifdef(CONFIG_AWS_IOT_PROVISION_CERTIFICATES certs)
