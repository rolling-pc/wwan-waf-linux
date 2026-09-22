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

project(log VERSION 1.0.0 LANGUAGES CXX)
find_package(Qt5 REQUIRED COMPONENTS Core)

set(CMAKE_RELEASE_DIR ${CMAKE_BINARY_DIR}/release_lib)
set(LIBRARY_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/adapter_os)
set(HEADER_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/adapter_os/api)  # 头文件的目标目录
set(SYMLINK_PATH ${CMAKE_RELEASE_DIR}/adapter_os/lib${PROJECT_NAME}.so.${PROJECT_VERSION})


set(LOG_MAJOR_VERSION 1)
set(LOG_MINOR_VERSION 0)
set(LOG_PATCH_VERSION 0)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LOG_VERSION_STRING ${LOG_MAJOR_VERSION}.${LOG_MINOR_VERSION}.${LOG_PATCH_VERSION})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
	add_compile_definitions(_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR) 
    set(LOG_VERSION_STRING ${LOG_MAJOR_VERSION}.${LOG_MINOR_VERSION}.${LOG_PATCH_VERSION}.0)
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
                "./version.h")

# 定义源码文件
set(build_file 
log.cpp
cliLog.cpp
)


set(build_head_path ${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/../api
                    ${CMAKE_RELEASE_DIR}/
                    ${CMAKE_RELEASE_DIR}/adapter_product/api
                    ${CMAKE_RELEASE_DIR}/adapter_os/api
                    ${CMAKE_SOURCE_DIR}/third_party/common/spdlog-1.x/include
                    )
# message(${CMAKE_CURRENT_BINARY_DIR}/../adb_lib)
set(local_lib_path  ${CMAKE_RELEASE_DIR}/base
                    ${CMAKE_RELEASE_DIR}/serial_port
                    ${CMAKE_RELEASE_DIR}/adapter_os
                    ${CMAKE_RELEASE_DIR}/adapter_product
                   )

set(HEADER_FILES  
    ${CMAKE_CURRENT_SOURCE_DIR}/../api/cliLog.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../api/log.hpp
)

include_directories(${build_head_path})

add_library(${PROJECT_NAME} SHARED ${build_file})

# 设置库的头文件路径
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${build_head_path}>
    $<INSTALL_INTERFACE:include>
)

# 链接到其他库
target_link_libraries(${PROJECT_NAME} Qt5::Core)
# 设置库的输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH}
)
target_compile_definitions(log PRIVATE LOG_DLL_EXPORTS)
# 自定义拷贝库文件、头文件和创建软链接
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	add_custom_command(
		TARGET ${PROJECT_NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${LIBRARY_OUTPUT_PATH}/lib${PROJECT_NAME}.so
	)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Windows 平台拷贝 .dll 和 .lib 文件
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${LIBRARY_OUTPUT_PATH}/log.dll
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_LINKER_FILE:${PROJECT_NAME}> ${LIBRARY_OUTPUT_PATH}/log.lib
    )
endif()
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
