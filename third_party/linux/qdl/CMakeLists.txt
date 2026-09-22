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

project(qdl VERSION 1.0.1 LANGUAGES C)
find_package(LibXml2 REQUIRED)

set(CMAKE_RELEASE_DIR ${CMAKE_BINARY_DIR}/release_lib)
set(LIBRARY_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/adapter_product/tools/qc/)
set(HEADER_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/third_party/api)  # 头文件的目标目录
set(SYMLINK_PATH ${CMAKE_RELEASE_DIR}/third_party/lib${PROJECT_NAME}.so.${PROJECT_VERSION})


set(qdl_source_files firehose.c qdl.c sahara.c util.c patch.c program.c ufs.c)

set(ks_source_files ks.c sahara.c util.c)

include_directories()

add_executable(ks ${ks_source_files})
add_executable(${PROJECT_NAME} ${qdl_source_files})

# =============== 显式设置 qdl 目标的 RPATH ===============
set_target_properties(${PROJECT_NAME} PROPERTIES
    INSTALL_RPATH "/opt/waf/libs/cxx_lib/"
    BUILD_WITH_INSTALL_RPATH TRUE
)
# ========================================================

target_compile_options(${PROJECT_NAME} PRIVATE -w)
target_link_libraries(${PROJECT_NAME} LibXml2::LibXml2 udev)
target_link_libraries(ks LibXml2::LibXml2 udev)

# 设置库的头文件路径
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# 设置库的输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH}
)

# 自定义拷贝库文件
add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${LIBRARY_OUTPUT_PATH}/${PROJECT_NAME}
)
