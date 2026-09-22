#cmake_minimum_required(VERSION 3.1)
#project(LPA_SDK VERSION 1.0.0 LANGUAGES CXX)
#include(GNUInstallDirs)
#
#message("System information")
#message(" - System Info file : ${CMAKE_SYSTEM_INFO_FILE}")
#message(" - System name      : ${CMAKE_SYSTEM_NAME}")
#message(" - System processor : ${CMAKE_SYSTEM_PROCESSOR}")
#message(" - System           : ${CMAKE_SYSTEM}")
#
## Set this variable OFF to use generic modem, else ON to use PCSC reader
#set(WINSCARD OFF)
#
##If needed, list of options can be updated by modifying target line below, depending system / interface
#if(CYGWIN)
#message("Generate Makefile for Cygwin")
#if(WINSCARD)
#message(" - Setting Makefile to use PCSC reader (winscard, pcsclite...)")
#add_definitions(-DLPA_SDK__PLATFORM_CYGWIN -DLPA_SDK__SEMEDIA_DRIVER_WINSCARD)
#else()
#message(" - Setting Makefile to use Generic Modem")
#add_definitions(-DLPA_SDK__PLATFORM_CYGWIN -DLPA_SDK__SEMEDIA_DRIVER_GENERIC_MODEM -DLPA_SDK__MAX_SIZE_OF_DATA_IN_STORE_DATA_APDU=255 -DLPA_SDK__NUMBER_OF_RETRY_FOR_CHAINED_GET_RESPONSE=2)
#endif()
#else()
#message("Generate Makefile for other system than Cygwin")
#if(WINSCARD)
#message(" - Setting Makefile to use PCSC reader (winscard, pcsclite...)")
#add_definitions(-DLPA_SDK__PLATFORM_RASPBIAN -DLPA_SDK__SEMEDIA_DRIVER_WINSCARD)
#else()
#message(" - Setting Makefile to use Generic Modem")
#add_definitions(-DLPA_SDK__PLATFORM_RASPBIAN -DLPA_SDK__SEMEDIA_DRIVER_GENERIC_MODEM -DLPA_SDK__MAX_SIZE_OF_DATA_IN_STORE_DATA_APDU=255 -DLPA_SDK__NUMBER_OF_RETRY_FOR_CHAINED_GET_RESPONSE=2)
#endif()
#endif()
#
#if(WINSCARD)
#set(SEMEDIA_SOURCE_C "semedia_winscard.c")
#else()
#set(SEMEDIA_SOURCE_C "semedia_genericmodem.c")
#endif()
#
#add_library(LPA_SDK STATIC
#Extra/base64/base64.c
#Extra/cJSON/cJSON.c
#Extra/sha256/sha-256.c
#LPA_SDK/src/lpasdk_api.c
#LPA_SDK/src/lpasdk_api_error.c
#LPA_SDK/src/core/bertlv_object.c
#LPA_SDK/src/core/httpmedia_base.c
#LPA_SDK/src/core/httpmedia_manager.c
#LPA_SDK/src/core/isdr_applet_manager.c
#LPA_SDK/src/core/lpa_config_file.c
#LPA_SDK/src/core/lpa_log.c
#LPA_SDK/src/core/lpa_manager.c
#LPA_SDK/src/core/lpa_manager_api.c
#LPA_SDK/src/core/lpa_manager_es9plus.c
#LPA_SDK/src/core/lpa_manager_es10b.c
#LPA_SDK/src/core/lpa_manager_es10c.c
#LPA_SDK/src/core/lpa_manager_helper.c
#LPA_SDK/src/core/lpa_memory.c
#LPA_SDK/src/core/rawdata_object.c
#LPA_SDK/src/core/semedia_base.c
#LPA_SDK/src/core/semedia_manager.c
#LPA_SDK/src/core/util.c
#LPA_SDK/src/driver/httpmedia_curl.c
#LPA_SDK/src/driver/${SEMEDIA_SOURCE_C}
#Sample_exe.cpp
#)
#
#
#include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include
#${CMAKE_CURRENT_SOURCE_DIR}/LPA_SDK/include
#${CMAKE_CURRENT_SOURCE_DIR}/LPA_SDK/src/driver
#${CMAKE_CURRENT_SOURCE_DIR}/LPA_SDK/src/core
#${CMAKE_CURRENT_SOURCE_DIR}/LPA_SDK/src
#${CMAKE_CURRENT_SOURCE_DIR}/Extra
#/usr/include/PCSC
#
#)
#
#
#set_property(TARGET LPA_SDK PROPERTY C_STANDARD 99)
#
#if(WINSCARD)
#if(CYGWIN)
#message("Link PCSC using winscard (Cygwin)")
#TARGET_LINK_LIBRARIES(LPA_SDK winscard)
#else()
#message("Link PCSC using pcsclite")
#TARGET_LINK_LIBRARIES(LPA_SDK pcsclite)
#endif()
#endif()
#
#find_package(Qt5 REQUIRED COMPONENTS Core)
#
#set(build_head_path ${CMAKE_CURRENT_SOURCE_DIR}
#        ${PROJECT_BINARY_DIR}/
#        ${CMAKE_RELEASE_DIR}/adapter_product/api
#        ${CMAKE_RELEASE_DIR}/adapter_os/api
#        ${CMAKE_SOURCE_DIR}/src/afal/adapter_product/api
#        ${CMAKE_SOURCE_DIR}/src/afal/adapter_os/api
#        ${CMAKE_SOURCE_DIR}/third_party/common/spdlog-1.x/include
#)
## message(${CMAKE_CURRENT_BINARY_DIR}/../adb_lib)
#set(local_lib_path  ${CMAKE_RELEASE_DIR}/adapter_os
#        ${CMAKE_RELEASE_DIR}/adapter_product
#        ${CMAKE_RELEASE_DIR}/private_func
#        ${CMAKE_RELEASE_DIR}/wwan_dev_lib
#)
#
#
#include_directories(${build_head_path})
#
#link_directories(${local_lib_path})
#
## Appear to be useless with Raspbian, needed with Cygwin
## So finally leaved by default to be sure it can work on all systems
## Eventually, in a target of optimization, you can try disabling following line
#TARGET_LINK_LIBRARIES(LPA_SDK curl mbim)
