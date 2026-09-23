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

# 项目信息
project(wwan_dev_service VERSION 1.0 LANGUAGES CXX)
find_package(Qt5 REQUIRED COMPONENTS Core Network)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
#set(CMAKE_BINARY_DIR "D:/2-Development/CBB_WAF/WAF/build")

configure_file(
    "${CMAKE_SOURCE_DIR}/version/version.h.in"      # 输入：项目根目录下的模板
    "${CMAKE_CURRENT_BINARY_DIR}/version.h"         # 输出：当前子项目的构建目录
    @ONLY                                           # 使用 @VAR@ 语法
)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Windows 下需要生成 .rc 文件 (用于资源)
    configure_file(
        "${CMAKE_SOURCE_DIR}/version/version.rc.in"      # 输入：项目根目录下的模板
        "${CMAKE_CURRENT_BINARY_DIR}/version.rc"         # 输出：当前子项目的构建目录
        @ONLY                                            # 使用 @VAR@ 语法
    )
endif()

# 添加子工程
add_subdirectory(wwan_dev_lib)
# ===== [开源改造 START] 私有库：内部仓库有源码 → 编译；对外仓库无源码 → 跳过 =====
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/private_dev/CMakeLists.txt)
add_subdirectory(private_dev)
else()
include(${CMAKE_SOURCE_DIR}/cmake/WafPrivateDeps.cmake)
    # 无私有源码时：接入 prebuilt 里的预编译库
    waf_import_private_lib(private_dev)
endif()
# ===== [开源改造 END] =====

add_definitions(-DWWAN_DEV_VERSION_STRING="${WWAN_DEV_VERSION_STRING}")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
        "./version.h")

# 设置构建路径和包含路径
set(CMAKE_RELEASE_DIR ${CMAKE_BINARY_DIR}/release_lib)

list(APPEND build_head_path
    ${CMAKE_CURRENT_BINARY_DIR}  
)

set(build_head_path ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_BINARY_DIR}/
        ${CMAKE_RELEASE_DIR}/
        ${CMAKE_RELEASE_DIR}/private_dev/api
        ${CMAKE_RELEASE_DIR}/adapter_product/api
        ${CMAKE_RELEASE_DIR}/adapter_os/api
        ${CMAKE_CURRENT_SOURCE_DIR}/../../afal/adapter_product/api
        ${CMAKE_CURRENT_SOURCE_DIR}/../../afal/adapter_os/api
        ${CMAKE_CURRENT_SOURCE_DIR}/wwan_dev_lib/api
		#${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party/common/spdlog-1.x/include
)

set(local_lib_path  ${CMAKE_RELEASE_DIR}/adapter_os
        ${CMAKE_RELEASE_DIR}/adapter_product
        ${CMAKE_RELEASE_DIR}/private_dev/
        ${CMAKE_RELEASE_DIR}/wwan_dev_lib/
		#${CMAKE_CURRENT_SOURCE_DIR}/../../../../build/release_lib/adapter_os #local test
)

include_directories(${build_head_path})

# 设置 RPATH 和系统库路径
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_CXX_CLANG_TIDY "")
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../../libs/adapter_product;$ORIGIN/../../libs/adapter_os;
                            $ORIGIN/../../libs/private_dev;$ORIGIN/../../libs/wwan_dev_lib;
                            $ORIGIN/../../libs/qt_lib")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(project_sys_lib_path "")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(project_sys_lib_path "")
endif()

# 定义项目的源文件

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(build_file linux_service.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	add_definitions(-DUNICODE -D_UNICODE)
	set(build_file windows_service.cpp)
endif()

# ===== [开源改造 START] 对外仓库无 main 源码 → 跳过可执行文件（服务以二进制交付）=====
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${build_file})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(${PROJECT_NAME} ${build_file})
    # Linux 下如果需要 version.h，确保上面 include_directories 包含了它
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # add_definitions(-DUNICODE -D_UNICODE)
    # Windows 下，在 add_executable 时直接包含 .rc 文件
    add_executable(${PROJECT_NAME} ${build_file} ${CMAKE_CURRENT_BINARY_DIR}/version.rc) # <-- 正确方式
endif()

### 定义可执行文件，并确保在编译前执行生成代码的脚本
##add_executable(${PROJECT_NAME} ${build_file})
##target_compile_options(${PROJECT_NAME} PRIVATE -fPIC)
if(NOT MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE -fPIC)
endif()

link_directories(${local_lib_path})
# 链接库
# 末尾的 osinfo 是预编译 private_dev 所需的传递依赖
target_link_libraries(${PROJECT_NAME} Qt5::Core Qt5::Network private_dev devicemonitor log osinfo)

# 添加客户端可执行文件
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
#add_executable(demo demo.cpp)
#target_compile_options(demo PRIVATE -fPIC)
#target_link_libraries(demo Qt5::Core Qt5::Network wwan_dev_lib log)
endif()
endif()
# ===== [开源改造 END] =====
