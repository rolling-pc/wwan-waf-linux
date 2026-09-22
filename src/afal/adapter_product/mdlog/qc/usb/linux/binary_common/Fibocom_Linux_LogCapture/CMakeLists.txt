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

set(CMAKE_CXX_CLANG_TIDY "")

project(logtool VERSION 1.0.0 LANGUAGES CXX)
# find_package(LibXml2 REQUIRED)

set(CMAKE_RELEASE_DIR ${CMAKE_BINARY_DIR}/release_lib)
set(LIBRARY_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/adapter_product/tools/qc/)
set(HEADER_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/third_party/api)  # 头文件的目标目录
set(SYMLINK_PATH ${CMAKE_RELEASE_DIR}/third_party/lib${PROJECT_NAME}.so.${PROJECT_VERSION})


set(qlogtool_source_files main.c proxy.c qcom_code/dump_api.c qcom_code/ftp_client.c 
                        qcom_code/qcom_log_main.c qcom_code/qlog.c qcom_code/qshrink4.c 
                        qcom_code/qtftp.c  qcom_code/sahara.c )
set(misc_source_files misc_code/fifo.c misc_code/log_control.c 
                        misc_code/misc_usb.c misc_code/misc.c )

set(build_head_path 
    qcom_code/ qcom_code/config misc_code/
)
include_directories(${build_head_path})

add_executable(${PROJECT_NAME} ${qlogtool_source_files} ${misc_source_files})
target_compile_definitions(${PROJECT_NAME} PRIVATE CONFIG_QCOM)

target_compile_definitions(${PROJECT_NAME} PRIVATE CONFIG_QCOM_DUMP=1
)
target_link_libraries(${PROJECT_NAME} z)
# 设置库的头文件路径
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${build_head_path}>
    $<INSTALL_INTERFACE:include>
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH}
)

# 自定义拷贝库文件、头文件和创建软链接
add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${LIBRARY_OUTPUT_PATH}/${PROJECT_NAME}
)
