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

# 项目信息
project(wwan_func_service VERSION 1.0 LANGUAGES CXX)

find_package(Qt5 REQUIRED COMPONENTS Core Network Concurrent)
set(CMAKE_AUTOMOC ON)

add_subdirectory(wwan_func_lib)
#if (INSTALL_TOOLS_ENABLE OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(wwan_func_lib_tool)
#endif()
# ===== [开源改造 START] 私有库：内部仓库有源码 → 编译；对外仓库无源码 → 跳过 =====
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/private_func/CMakeLists.txt)
add_subdirectory(private_func)
else()
include(${CMAKE_SOURCE_DIR}/cmake/WafPrivateDeps.cmake)
    # 无私有源码时：接入 prebuilt 里的预编译库
    waf_import_private_lib(private_func)
endif()
# ===== [开源改造 END] =====

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

# if(FUNCTION_FLASH_ENABLE)
#     add_compile_definitions(FLASH_ENABLE=1)
# endif()
# if(FUNCTION_CONFIG_ENABLE)
#     add_compile_definitions(CONFIG_ENABLE=1)
# endif()
# if(FUNCTION_ESIM_LPA_ENABLE)
#     add_compile_definitions(ESIM_LPA_ENABLE=1)
# endif()
# if(FUNCTION_MA_ENABLE)
#     add_compile_definitions(MA_ENABLE=1)
# endif()

# # 添加可执行文件
set(CMAKE_RELEASE_DIR ${CMAKE_BINARY_DIR}/release_lib)

set(build_head_path ${CMAKE_CURRENT_SOURCE_DIR}
                    ${PROJECT_BINARY_DIR}/
                    ${CMAKE_RELEASE_DIR}/
                    ${CMAKE_RELEASE_DIR}/adapter_product/api
                    ${CMAKE_RELEASE_DIR}/adapter_os/api
                    ${CMAKE_RELEASE_DIR}/private_func/api
                    ${CMAKE_CURRENT_SOURCE_DIR}/../../afal/adapter_os/api
                    ${CMAKE_CURRENT_SOURCE_DIR}/../../afal/adapter_product/api
                    ${CMAKE_CURRENT_SOURCE_DIR}/private_func/api
                    ${CMAKE_CURRENT_SOURCE_DIR}/private_func/src
                    )
# message(${CMAKE_CURRENT_BINARY_DIR}/../adb_lib)
set(local_lib_path  ${CMAKE_RELEASE_DIR}/adapter_os
                    ${CMAKE_RELEASE_DIR}/adapter_product
                    ${CMAKE_RELEASE_DIR}/private_func
                    ${CMAKE_RELEASE_DIR}/wwan_dev_lib
                    ${CMAKE_RELEASE_DIR}/third_party
                    ${CMAKE_RELEASE_DIR}/curl_lib
                    ${CMAKE_RELEASE_DIR}/openssl_lib
                   )


include_directories(${build_head_path})

link_directories(${local_lib_path})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../../libs/adapter_product;$ORIGIN/../../libs/adapter_os;
                            $ORIGIN/../../libs/qt_lib;$ORIGIN/../../libs/private_func;
                            $ORIGIN/../../libs/wwan_dev_lib")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(build_file linux_service.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(project_sys_lib_path "")
    set(build_file windows_service.cpp)
endif()

# ===== [开源改造 START] 对外仓库无 main 源码 → 跳过可执行文件（服务以二进制交付）=====
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${build_file})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(${PROJECT_NAME} ${build_file})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")    
    add_executable(${PROJECT_NAME} ${build_file} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

##add_executable(${PROJECT_NAME} ${build_file})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(${PROJECT_NAME} PRIVATE -fPIC)
endif()
# 链接导入的依赖项
# 末尾几项是预编译 private_func 所需的传递依赖
target_link_libraries(${PROJECT_NAME} Qt5::Core Qt5::Network Qt5::Concurrent mbim log private_func wwan_dev_lib osinfo display power lpa QuaZip curl)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # **确保静态库的路径**
    target_link_directories(${PROJECT_NAME} PRIVATE ${CMAKE_RELEASE_DIR}/private_func ${CMAKE_RELEASE_DIR}/wwan_dev_lib)
endif()
endif()
# ===== [开源改造 END] =====
