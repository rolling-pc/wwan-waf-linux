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

project(wwan_dev_lib VERSION 1.0.0 LANGUAGES CXX)
find_package(Qt5 REQUIRED COMPONENTS Core Network)
set(CMAKE_AUTOMOC ON)

set(CMAKE_RELEASE_DIR ${CMAKE_BINARY_DIR}/release_lib)
set(LIBRARY_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/wwan_dev_lib/)
set(HEADER_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/wwan_dev_lib/api)  # 头文件的目标目录
set(SYMLINK_PATH ${CMAKE_BINARY_DIR}/wwan_dev_lib/lib${PROJECT_NAME}.so.${PROJECT_VERSION})


set(WWAN_DEV_LIB_MAJOR_VERSION 1)
set(WWAN_DEV_LIB_MINOR_VERSION 0)
set(WWAN_DEV_LIB_PATCH_VERSION 0)

qt5_wrap_cpp(MOC_FILES ./api/Client.h ClientImpl.h )

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(WWAN_DEV_LIB_VERSION_STRING ${WWAN_DEV_LIB_MAJOR_VERSION}.${WWAN_DEV_LIB_MINOR_VERSION}.${WWAN_DEV_LIB_PATCH_VERSION})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(WWAN_DEV_LIB_VERSION_STRING ${WWAN_DEV_LIB_MAJOR_VERSION}.${WWAN_DEV_LIB_MINOR_VERSION}.${WWAN_DEV_LIB_PATCH_VERSION}.0)
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
                "./version.h")
# 定义源码文件
set(build_file 
    Client.cpp ./api/Client.h ClientImpl.cpp ClientImpl.h
)

# 定义头文件路径
set(build_head_path
    ${CMAKE_CURRENT_BINARY_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/api
	${CMAKE_CURRENT_SOURCE_DIR}/../../../afal/adapter_os/api
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/common/spdlog-1.x/include #local test
)

# 定义头文件路径
set(HEADER_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/api  # 假设头文件在 include 目录下
)

include_directories(${build_head_path})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_CXX_CLANG_TIDY "")
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../../libs/adapter_product;$ORIGIN/../../libs/adapter_os;
                            $ORIGIN/../../libs/qt_lib;$ORIGIN/../../libs/cxx_lib")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(CMAKE_SKIP_BUILD_RPATH FALSE)
    set(project_sys_lib_path ""
                            )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(project_sys_lib_path "")
endif()


set(local_lib_path  ${CMAKE_RELEASE_DIR}/adapter_os
                    ${CMAKE_RELEASE_DIR}/adapter_product
                    ${CMAKE_RELEASE_DIR}/cxx_lib
                   )
link_directories(${local_lib_path})

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_library(${PROJECT_NAME} STATIC ${build_file} ${MOC_FILES})
else()
    add_library(${PROJECT_NAME} SHARED ${build_file} ${MOC_FILES})
endif()
target_link_libraries(${PROJECT_NAME} Qt5::Core Qt5::Network log)
# 设置库的头文件路径
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${build_head_path}>
    $<INSTALL_INTERFACE:include>
)

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
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${HEADER_FILES} ${HEADER_OUTPUT_PATH}
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${LIBRARY_OUTPUT_PATH}/lib${PROJECT_NAME}.lib
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${HEADER_FILES} ${HEADER_OUTPUT_PATH}
    )
endif()