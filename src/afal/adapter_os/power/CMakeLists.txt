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

project(power VERSION 1.0.0 LANGUAGES CXX)
find_package(Qt5 REQUIRED COMPONENTS Core)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(DBUS1 REQUIRED dbus-1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    #
endif()

set(CMAKE_AUTOMOC ON)

set(CMAKE_RELEASE_DIR ${CMAKE_BINARY_DIR}/release_lib)
set(LIBRARY_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/adapter_os)
set(HEADER_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/adapter_os/api)  # 头文件的目标目录
set(SYMLINK_PATH ${CMAKE_RELEASE_DIR}/adapter_os/lib${PROJECT_NAME}.so.${PROJECT_VERSION})


set(POWER_MAJOR_VERSION 1)
set(POWER_MINOR_VERSION 0)
set(POWER_PATCH_VERSION 0)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(POWER_VERSION_STRING ${POWER_MAJOR_VERSION}.${POWER_MINOR_VERSION}.${POWER_PATCH_VERSION})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(POWER_VERSION_STRING ${POWER_MAJOR_VERSION}.${POWER_MINOR_VERSION}.${POWER_PATCH_VERSION}.0)
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
                "./version.h")

add_definitions(-DUNICODE -D_UNICODE)

# 定义源码文件
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(build_file linux/power.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(build_file windows/power.cpp)
endif()

set(build_head_path ${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/../api
                    ${CMAKE_RELEASE_DIR}/
                    ${CMAKE_RELEASE_DIR}/adapter_product/api
                    ${CMAKE_RELEASE_DIR}/adapter_os/api
                    ${DBUS1_INCLUDE_DIRS}
                    )
# message(${CMAKE_CURRENT_BINARY_DIR}/../adb_lib)
set(local_lib_path  ${CMAKE_RELEASE_DIR}/adapter_os
                    ${CMAKE_RELEASE_DIR}/adapter_product
                    )

set(HEADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/../api/power.hpp)

include_directories(${build_head_path})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../../libs/qt_lib")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(project_sys_lib_path "")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(project_sys_lib_path "")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_library(${PROJECT_NAME} SHARED ${build_file})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_library(${PROJECT_NAME} STATIC ${build_file})
endif()
target_link_libraries(${PROJECT_NAME} Qt5::Core ${DBUS1_LIBRARIES})
# 设置库的头文件路径
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${build_head_path}>
    $<INSTALL_INTERFACE:include>
)

# 链接到其他库
# target_link_libraries(${PROJECT_NAME} base)
# 设置库的输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH}
)

# 自定义拷贝库文件、头文件和创建软链接
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	add_custom_command(
		TARGET ${PROJECT_NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${LIBRARY_OUTPUT_PATH}/lib${PROJECT_NAME}.so
	)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	add_custom_command(
		TARGET ${PROJECT_NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_LINKER_FILE:${PROJECT_NAME}> ${LIBRARY_OUTPUT_PATH}/lib${PROJECT_NAME}.lib
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
