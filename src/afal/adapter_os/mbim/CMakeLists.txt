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

project(mbim VERSION 1.0.0 LANGUAGES CXX)
find_package(Qt5 REQUIRED COMPONENTS Core)
set(CMAKE_AUTOMOC ON)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_RELEASE_DIR ${CMAKE_BINARY_DIR}/release_lib)
set(LIBRARY_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/adapter_os)
set(HEADER_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/adapter_os/api)  # 头文件的目标目录
set(SYMLINK_PATH ${CMAKE_RELEASE_DIR}/adapter_os/lib${PROJECT_NAME}.so.${PROJECT_VERSION})


set(MBIM_MAJOR_VERSION 1)
set(MBIM_MINOR_VERSION 0)
set(MBIM_PATCH_VERSION 0)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(MBIM_VERSION_STRING ${MBIM_MAJOR_VERSION}.${MBIM_MINOR_VERSION}.${MBIM_PATCH_VERSION})
    # 定义源码文件
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	add_compile_definitions(_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR) 
    set(MBIM_VERSION_STRING ${MBIM_MAJOR_VERSION}.${MBIM_MINOR_VERSION}.${MBIM_PATCH_VERSION}.0)
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
                "./version.h")

# 定义源码文件
#set(build_file
#        mbim.cpp
#)

set(build_head_path ${CMAKE_CURRENT_SOURCE_DIR}
                    ${PROJECT_BINARY_DIR}/
                    ${CMAKE_RELEASE_DIR}/adapter_product/api
                    ${CMAKE_RELEASE_DIR}/adapter_os/api
                    ${CMAKE_CURRENT_SOURCE_DIR}/../api
                    ${CMAKE_SOURCE_DIR}/third_party/common/spdlog-1.x/include
                    #${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/common/spdlog-1.x/include 
                    )
# message(${CMAKE_CURRENT_BINARY_DIR}/../adb_lib)
set(local_lib_path  ${CMAKE_RELEASE_DIR}/adapter_os
                    ${CMAKE_RELEASE_DIR}/adapter_product
                    ${CMAKE_RELEASE_DIR}/cxx_lib
                   )
link_directories(${local_lib_path})

set(HEADER_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../api/mbim.hpp
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_CXX_CLANG_TIDY "")
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../../libs/qt_lib;$ORIGIN/../../libs/cxx_lib")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(CMAKE_SKIP_BUILD_RPATH FALSE)
    set(project_sys_lib_path "")
    find_package(PkgConfig REQUIRED)
    find_package(Qt5 REQUIRED COMPONENTS Core)
    pkg_check_modules(DEPS REQUIRED IMPORTED_TARGET glib-2.0 gio-2.0 gio-unix-2.0 mbim-glib)
    set(build_file
            ${build_file}
            linux/mbim.cpp
            linux/mbim_linux_helper_basic_func.cpp
            linux/mbim_linux_helper_adapter.cpp
            linux/mbim_linux_helper_api.cpp
    )
    set(build_head_path
            ${build_head_path}
            linux
    )

    message(STATUS "glib-2.0 version: ${DEPS_VERSION}")
    message(STATUS "glib-2.0 include dirs: ${DEPS_INCLUDE_DIRS}")
    message(STATUS "glib-2.0 libraries: ${DEPS_LIBRARIES}")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(project_sys_lib_path "")
    find_package(Qt5 REQUIRED COMPONENTS Core)
    set(build_file
            ${build_file}
            windows/mbim.cpp
            windows/MbimSender.cpp
            windows/MBIM_DLL.cpp
            windows/CMbnSink.cpp
    )
    set(build_head_path
            ${build_head_path}
            windows
    )

endif()

include_directories(${build_head_path})
#include_directories( /usr/include/glib-2.0 /usr/lib/x86_64-linux-gnu/glib-2.0/include/)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_library(${PROJECT_NAME} SHARED ${build_file})
    target_link_libraries(${PROJECT_NAME} PkgConfig::DEPS Qt5::Core)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_library(${PROJECT_NAME} STATIC ${build_file})

    if(MSVC)
        if(WAF_TARGET_ARCH STREQUAL "arm64")
            set(_ATL_LIB_DIR "$ENV{VCToolsInstallDir}atlmfc/lib/arm64")
        elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(_ATL_LIB_DIR "$ENV{VCToolsInstallDir}atlmfc/lib/x64")
        else()
            set(_ATL_LIB_DIR "$ENV{VCToolsInstallDir}atlmfc/lib/x86")
        endif()
        find_library(ATLS_LIB NAMES atls PATHS ${_ATL_LIB_DIR} NO_DEFAULT_PATH)
        if(NOT ATLS_LIB)
            message(FATAL_ERROR
                "atls.lib not found under ${_ATL_LIB_DIR}. Install Visual Studio component: "
                "'C++ ATL for latest v143 build tools' (include ARM64 when building arm64).")
        endif()
        target_include_directories(${PROJECT_NAME} PRIVATE "$ENV{VCToolsInstallDir}atlmfc/include")
    endif()

	target_link_libraries(${PROJECT_NAME}
			PUBLIC
				windowsapp
				runtimeobject
				Qt5::Core
				IPHLPAPI
				setupapi
			PRIVATE
				osinfo
				log
				mbnapi_uuid
	)
    if(MSVC)
        # MBIM_DLL.cpp / CMbnSink.cpp use CComPtr from atlbase.h
        target_link_libraries(${PROJECT_NAME} PUBLIC ${ATLS_LIB})
    endif()
endif()

# 设置库的头文件路径
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${build_head_path}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${PROJECT_NAME})
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


# 添加客户端可执行文件
#if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
#add_executable(mbimdemo demo.cpp
#        ${CMAKE_SOURCE_DIR}/src/afal/adapter_product/api
#        ${CMAKE_SOURCE_DIR}/src/afal/adapter_os/api
#        ${CMAKE_SOURCE_DIR}/src/afal/adapter_os/mbim/linux
#)
#target_compile_options(mbimdemo PRIVATE -fPIC)
#target_link_libraries(mbimdemo Qt5::Core PkgConfig::DEPS mbim log)
#elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
#add_executable(mbimdemo demo.cpp
#        ${CMAKE_SOURCE_DIR}/src/afal/adapter_product/api
#        ${CMAKE_SOURCE_DIR}/src/afal/adapter_os/api
#        ${CMAKE_SOURCE_DIR}/src/afal/adapter_os/mbim/windows
#)
#target_compile_options(mbimdemo PRIVATE -fPIC)
#target_link_libraries(mbimdemo Qt5::Core mbim log)
#endif()