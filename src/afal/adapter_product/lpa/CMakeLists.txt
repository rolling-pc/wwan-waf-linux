# /* This program is free software: you can redistribute it and/or modify
#  * it under the terms of the GNU Lesser General Public License as published by
#  * the Free Software Foundation, either version 3 of the License, or
#  * (at your option) any later version.
#  *
#  * This program is distributed in the hope that it will be useful,
#  * but WITHOUT ANY WARRANTY; without even the implied warranty of
#  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  * GNU Lesser General Public License for more details.
#  *
#  * You should have received a copy of the GNU Lesser General Public License
#  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
#  *
#  * Copyright (C) 2024, Rolling Wireless S.a.r.l.
#  */

cmake_minimum_required(VERSION 3.22)
project(lpa VERSION 1.0.0 LANGUAGES CXX)

# 设置安装路径为当前构建目录下的 install 子目录

# Add directory compilation
set(DEST_DIR "${CMAKE_BINARY_DIR}/release_lib")

find_package(Qt5 REQUIRED COMPONENTS Core)
set(CMAKE_AUTOMOC ON)

set(CMAKE_RELEASE_DIR ${CMAKE_BINARY_DIR}/release_lib)
set(LIBRARY_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/adapter_product)
set(HEADER_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/adapter_os/api)  # 头文件的目标目录
set(SYMLINK_PATH ${CMAKE_RELEASE_DIR}/adapter_product/lib${PROJECT_NAME}.so.${PROJECT_VERSION})

set(LPA_MAJOR_VERSION 1)
set(LPA_MINOR_VERSION 0)
set(LPA_PATCH_VERSION 0)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LPA_VERSION_STRING ${LPA_MAJOR_VERSION}.${LPA_MINOR_VERSION}.${LPA_PATCH_VERSION})
    # 定义源码文件
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(LPA_VERSION_STRING ${LPA_MAJOR_VERSION}.${LPA_MINOR_VERSION}.${LPA_PATCH_VERSION}.0)
endif()

#if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
#    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/linux")
#        set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/linux")
#        file(COPY "${SOURCE_DIR}/" DESTINATION "${DEST_DIR}")
#    else()
#        add_subdirectory(adapter_product)
#        add_subdirectory(adapter_os)
#    endif()
#elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
#    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/windows")
#        message("copy windows lib file")
#    else()
#        add_subdirectory(adapter_product)
#        add_subdirectory(adapter_os)
#    endif()
#endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
        "./version.h")
#add_subdirectory(common)

#add_library(lpa SHARED
#lpa.cpp
#)

message("System information")
message(" - System Info file : ${CMAKE_SYSTEM_INFO_FILE}")
message(" - System name      : ${CMAKE_SYSTEM_NAME}")
message(" - System processor : ${CMAKE_SYSTEM_PROCESSOR}")
message(" - System           : ${CMAKE_SYSTEM}")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	add_definitions(-DLPA_SDK__PLATFORM_RASPBIAN)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	add_definitions(-DLPA_SDK__PLATFORM_WIN -DCURL_STATICLIB)
endif()

add_definitions(-DLPA_SDK__SEMEDIA_DRIVER_GENERIC_MODEM -D_FFW_PCIOT_LPA_MODIFY_ -DLPA_SDK__MEMORY -DLPA_SDK__MEMORY_MONITORING -DLPA_SDK__MAX_SIZE_OF_DATA_IN_STORE_DATA_APDU=255 -DLPA_SDK__NUMBER_OF_RETRY_FOR_CHAINED_GET_RESPONSE=2)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include
${CMAKE_CURRENT_SOURCE_DIR}/common
${CMAKE_CURRENT_SOURCE_DIR}/common/LPA_SDK/include/
${CMAKE_CURRENT_SOURCE_DIR}/common/LPA_SDK/include/lpasdk/platform_adaptor
${CMAKE_CURRENT_SOURCE_DIR}/common/LPA_SDK/include/lpasdk/api
${CMAKE_CURRENT_SOURCE_DIR}/common/LPA_SDK/include/lpasdk/api/semedia
${CMAKE_CURRENT_SOURCE_DIR}/common/LPA_SDK/include/lpasdk/core
${CMAKE_CURRENT_SOURCE_DIR}/common/LPA_SDK/include/lpasdk/driver
${CMAKE_CURRENT_SOURCE_DIR}/common/Extra/
/usr/include/PCSC
)

find_package(Qt5 REQUIRED COMPONENTS Core)

set(build_head_path ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_BINARY_DIR}/
        ${CMAKE_RELEASE_DIR}/adapter_product/api
		${CMAKE_RELEASE_DIR}/adapter_os/api
        ${CMAKE_SOURCE_DIR}/src/afal/adapter_product/api
        ${CMAKE_SOURCE_DIR}/src/afal/adapter_os/api
		${CMAKE_RELEASE_DIR}/src/afal/adapter_os/api/curl
		${CMAKE_RELEASE_DIR}/src/afal/adapter_os/api/openssl
        ${CMAKE_SOURCE_DIR}/third_party/common/spdlog-1.x/include
)
# message(${CMAKE_CURRENT_BINARY_DIR}/../adb_lib)
set(local_lib_path  ${CMAKE_RELEASE_DIR}/adapter_os
        ${CMAKE_RELEASE_DIR}/adapter_product
        ${CMAKE_RELEASE_DIR}/wwan_dev_lib
        ${CMAKE_RELEASE_DIR}/curl_lib
        ${CMAKE_RELEASE_DIR}/openssl_lib
)

add_library(lpa STATIC
common/Extra/base64/base64.c
common/Extra/cJSON/cJSON.c
common/Extra/sha256/sha-256.c
common/LPA_SDK/src/lpasdk_api.c
common/LPA_SDK/src/lpasdk_api_error.c
common/LPA_SDK/src/core/bertlv_object.c
common/LPA_SDK/src/core/httpmedia_base.c
common/LPA_SDK/src/core/httpmedia_manager.c
common/LPA_SDK/src/core/isdr_applet_manager.c
common/LPA_SDK/src/core/lpa_config_file.c
common/LPA_SDK/src/core/lpa_log.c
common/LPA_SDK/src/core/lpa_manager.c
common/LPA_SDK/src/core/lpa_manager_api.c
common/LPA_SDK/src/core/lpa_manager_es9plus.c
common/LPA_SDK/src/core/lpa_manager_es10b.c
common/LPA_SDK/src/core/lpa_manager_es10c.c
common/LPA_SDK/src/core/lpa_manager_helper.c
common/LPA_SDK/src/core/lpa_memory.c
common/LPA_SDK/src/core/rawdata_object.c
common/LPA_SDK/src/core/semedia_base.c
common/LPA_SDK/src/core/semedia_manager.c
common/LPA_SDK/src/core/util.c
common/LPA_SDK/src/driver/httpmedia_curl.c
common/LPA_SDK/src/driver/semedia_genericmodem.c
common/LPA_SDK/src/platform_adaptor/plat_lpa_adaptor_fm350.cpp
common/LPA_SDK/src/platform_adaptor/plat_lpa_adaptor_qc151.cpp
common/Sample_exe.cpp
lpa.cpp
)

set_property(TARGET lpa PROPERTY C_STANDARD 99)

set(HEADER_FILES  
    ${CMAKE_CURRENT_SOURCE_DIR}/../api/lpa.hpp
)

# copy 所需头文件
foreach(HEADER_FILE ${HEADER_FILES})
    get_filename_component(HEADER_NAME ${HEADER_FILE} NAME)
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            ${HEADER_FILE}
            ${HEADER_OUTPUT_PATH}/${HEADER_NAME}
        COMMENT "Copying ${HEADER_NAME} to ${HEADER_OUTPUT_PATH}"
    )
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	TARGET_LINK_LIBRARIES(lpa Qt5::Core log mbim curl ssl crypto)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	TARGET_LINK_LIBRARIES(lpa Qt5::Core log mbim libcurl libcrypto libssl)	
endif()

include_directories(${build_head_path})

link_directories(${local_lib_path})

# 添加客户端可执行文件
add_executable(Sample_exe demo.cpp
        ${CMAKE_SOURCE_DIR}/src/afal/adapter_product/api
        ${CMAKE_SOURCE_DIR}/src/afal/adapter_os/api
        ${CMAKE_SOURCE_DIR}/src/afal/adapter_product/lpa/common
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(Sample_exe PRIVATE -fPIC)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_link_libraries(Sample_exe Qt5::Core mbim log z curl lpa)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_link_libraries(Sample_exe Qt5::Core mbim log libcurl crypt32.lib ws2_32.lib wldap32.lib libcrypto libssl lpa)
endif()
