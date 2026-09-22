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
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(wwan_func_tool VERSION 1.0.0 LANGUAGES CXX)
find_package(Qt5 REQUIRED COMPONENTS Core Network)
set(CMAKE_AUTOMOC ON)

set(CMAKE_RELEASE_DIR ${CMAKE_BINARY_DIR}/release_lib)
set(LIBRARY_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/wwan_func_lib_tool)
set(HEADER_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/wwan_func_lib_tool/api)  # 头文件的目标目录
set(SYMLINK_PATH ${CMAKE_RELEASE_DIR}/wwan_func_lib_tool/lib${PROJECT_NAME}.so.${PROJECT_VERSION})

qt5_wrap_cpp(MOC_FILES wwan_impl_tool.h ./api/wwan_func_tool.hpp)

set(WWAN_FUNC_LIB_MAJOR_VERSION 1)
set(WWAN_FUNC_LIB_MINOR_VERSION 0)
set(WWAN_FUNC_LIB_PATCH_VERSION 0)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(WWAN_FUNC_LIB_VERSION_STRING ${WWAN_FUNC_LIB_MAJOR_VERSION}.${WWAN_FUNC_LIB_MINOR_VERSION}.${WWAN_FUNC_LIB_PATCH_VERSION})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(WWAN_FUNC_LIB_VERSION_STRING ${WWAN_FUNC_LIB_MAJOR_VERSION}.${WWAN_FUNC_LIB_MINOR_VERSION}.${WWAN_FUNC_LIB_PATCH_VERSION}.0)
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
                "./version.h")

# 定义源码文件
set(build_file 
    wwan_func_tool.cpp
	wwan_impl_tool.h
    ./api/wwan_func_tool.hpp
)

# 定义头文件路径
set(build_head_path 
    ${CMAKE_CURRENT_SOURCE_DIR}/api  # 假设头文件在 include 目录下
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/common/spdlog-1.x/include
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../build/release_lib/adapter_os/api
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../build/release_lib/adapter_os
)

include_directories(${build_head_path})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_CXX_CLANG_TIDY "")
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../../libs/adapter_product;$ORIGIN/../../libs/adapter_os;
                            $ORIGIN/../../libs/qt_lib")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(project_sys_lib_path ""
                            )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(project_sys_lib_path "")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")	
    add_library(${PROJECT_NAME} SHARED ${build_file} ${MOC_FILES})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")	
    add_library(${PROJECT_NAME} STATIC ${build_file} ${MOC_FILES})
endif()

target_link_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../../build/release_lib/adapter_os) 
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

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(LIB_EXT "so")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(LIB_EXT "lib")
endif()

# 自定义拷贝库文件、头文件和创建软链接
add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${LIBRARY_OUTPUT_PATH}/lib${PROJECT_NAME}.${LIB_EXT}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${build_head_path} ${HEADER_OUTPUT_PATH}
)
