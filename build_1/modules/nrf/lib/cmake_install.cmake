# Install script for directory: /opt/nordic/ncs/v2.5.1/nrf/lib

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/opt/nordic/ncs/toolchains/20d68df7e5/opt/zephyr-sdk/arm-zephyr-eabi/bin/arm-zephyr-eabi-objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/opt/nordic/ncs/v2.5.1/nrf/applications/firmware/build_1/modules/nrf/lib/bin/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/opt/nordic/ncs/v2.5.1/nrf/applications/firmware/build_1/modules/nrf/lib/nrf_modem_lib/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/opt/nordic/ncs/v2.5.1/nrf/applications/firmware/build_1/modules/nrf/lib/at_monitor/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/opt/nordic/ncs/v2.5.1/nrf/applications/firmware/build_1/modules/nrf/lib/at_host/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/opt/nordic/ncs/v2.5.1/nrf/applications/firmware/build_1/modules/nrf/lib/at_cmd_parser/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/opt/nordic/ncs/v2.5.1/nrf/applications/firmware/build_1/modules/nrf/lib/lte_link_control/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/opt/nordic/ncs/v2.5.1/nrf/applications/firmware/build_1/modules/nrf/lib/modem_info/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/opt/nordic/ncs/v2.5.1/nrf/applications/firmware/build_1/modules/nrf/lib/pdn/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/opt/nordic/ncs/v2.5.1/nrf/applications/firmware/build_1/modules/nrf/lib/adp536x/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/opt/nordic/ncs/v2.5.1/nrf/applications/firmware/build_1/modules/nrf/lib/flash_patch/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/opt/nordic/ncs/v2.5.1/nrf/applications/firmware/build_1/modules/nrf/lib/fatal_error/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/opt/nordic/ncs/v2.5.1/nrf/applications/firmware/build_1/modules/nrf/lib/hw_id/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/opt/nordic/ncs/v2.5.1/nrf/applications/firmware/build_1/modules/nrf/lib/modem_antenna/cmake_install.cmake")
endif()

