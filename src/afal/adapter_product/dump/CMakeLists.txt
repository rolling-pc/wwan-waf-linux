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

include(${CMAKE_SOURCE_DIR}/cmake/WafArch.cmake)

set(CMAKE_CXX_STANDARD 20)
project(dump VERSION 1.0.0 LANGUAGES CXX)
# find_package(Qt5 REQUIRED COMPONENTS Core)
find_package(Qt5 REQUIRED COMPONENTS SerialPort Core Network)
set(CMAKE_AUTOMOC ON)

set(CMAKE_RELEASE_DIR ${CMAKE_BINARY_DIR}/release_lib)
set(LIBRARY_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/adapter_product)
set(HEADER_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/adapter_product/api)  # 头文件的目标目录
#set(SYMLINK_PATH ${CMAKE_RELEASE_DIR}/adapter_product/lib${PROJECT_NAME}.so.${PROJECT_VERSION})  //no reference below

set(DUMP_MAJOR_VERSION 1)
set(DUMP_MINOR_VERSION 0)
set(DUMP_PATCH_VERSION 0)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(DUMP_VERSION_STRING ${DUMP_MAJOR_VERSION}.${DUMP_MINOR_VERSION}.${DUMP_PATCH_VERSION})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(DUMP_VERSION_STRING ${DUMP_MAJOR_VERSION}.${DUMP_MINOR_VERSION}.${DUMP_PATCH_VERSION}.0)
	set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
                "./version.h")


# 定义源码文件
set(build_file 
dump.cc
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(build_file_mtk
	${build_file}
	mtk/pcie/linux/dump_mtk_pcie.cc
	mtk/usb/linux/dump_mtk_usb.cc
	)

	set(build_file_qc
	${build_file}
	qc/usb/linux/dump_qc.cc
	)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(build_file_mtk
    ${build_file}
	mtk/pcie/windows/dump_mtk_pcie_win.cc
    mtk/pcie/windows/dump_mtk_pcie_fulldump.cc
	)
	
	set(build_file_qc
	${build_file}
	qc/usb/windows/dump_qc.cc
	)
endif()

set(HEADER_FILES  
    ${CMAKE_CURRENT_SOURCE_DIR}/../api/dump_interface.h
)
# 定义头文件路径
set(build_head_path 
    ${CMAKE_CURRENT_SOURCE_DIR}/../api/
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_SOURCE_DIR}/third_party/common/spdlog-1.x/include
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../adapter_os/api/
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/common/spdlog-1.x/include
)

include_directories(${build_head_path})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../adapter_product;$ORIGIN/../adapter_os;$ORIGIN/../../libs/qt_lib;
                            $ORIGIN/../cxx_lib")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(project_sys_lib_path ""
                            )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(project_sys_lib_path "")
    qt5_wrap_cpp(MOC_SOURCES_MTK ${CMAKE_CURRENT_SOURCE_DIR}/../api/dump_interface.h mtk/pcie/windows/dump_mtk_pcie_fulldump.h mtk/pcie/windows/dump_mtk_pcie_win.h)
    qt5_wrap_cpp(MOC_SOURCES_QC ${CMAKE_CURRENT_SOURCE_DIR}/../api/dump_interface.h qc/usb/windows/dump_qc.h)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LIB_EXT "so")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(LIB_EXT "lib")
endif()

if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND WAF_TARGET_ARCH STREQUAL "arm64"))
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_library(${PROJECT_NAME}_mtk SHARED ${build_file_mtk})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_library(${PROJECT_NAME}_mtk SHARED ${build_file_mtk} ${MOC_SOURCES_MTK})
endif()

target_compile_definitions(${PROJECT_NAME}_mtk PRIVATE BUILD_MTK DUMP_VERSION_STRING="${DUMP_VERSION_STRING}")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${PROJECT_NAME}_mtk afal_common Qt5::Core)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(${PROJECT_NAME}_mtk afal_common Qt5::Core Qt5::SerialPort MiniDebugLogger)
endif()
# 设置库的头文件路径
target_include_directories(${PROJECT_NAME}_mtk PUBLIC
    $<BUILD_INTERFACE:${build_head_path}>
    $<INSTALL_INTERFACE:include>
)

# 设置库的输出目录
set_target_properties(${PROJECT_NAME}_mtk PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH}
)

# 自定义拷贝库文件、头文件
add_custom_command(
    TARGET ${PROJECT_NAME}_mtk
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}_mtk> ${LIBRARY_OUTPUT_PATH}/lib${PROJECT_NAME}_mtk.${LIB_EXT}
)
# copy 所需头文件
foreach(HEADER_FILE ${HEADER_FILES})
    get_filename_component(HEADER_NAME ${HEADER_FILE} NAME)
    add_custom_command(
        TARGET ${PROJECT_NAME}_mtk
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            ${HEADER_FILE}
            ${HEADER_OUTPUT_PATH}/${HEADER_NAME}
        COMMENT "Copying ${HEADER_NAME} to ${HEADER_OUTPUT_PATH}"
    )
endforeach()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_library(${PROJECT_NAME}_qc SHARED ${build_file_qc})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_library(${PROJECT_NAME}_qc SHARED ${build_file_qc} ${MOC_SOURCES_QC})
endif()

target_compile_definitions(${PROJECT_NAME}_qc PRIVATE BUILD_QC DUMP_VERSION_STRING="${DUMP_VERSION_STRING}")
target_link_libraries(${PROJECT_NAME}_qc afal_common Qt5::Core Qt5::Network)

# 设置库的头文件路径
target_include_directories(${PROJECT_NAME}_qc PUBLIC
    $<BUILD_INTERFACE:${build_head_path}>
    $<INSTALL_INTERFACE:include>
)

# 设置库的输出目录
set_target_properties(${PROJECT_NAME}_qc PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH}
)

# 自定义拷贝库文件、头文件
add_custom_command(
    TARGET ${PROJECT_NAME}_qc
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}_qc> ${LIBRARY_OUTPUT_PATH}/lib${PROJECT_NAME}_qc.${LIB_EXT}
)
# copy 所需头文件
foreach(HEADER_FILE ${HEADER_FILES})
    get_filename_component(HEADER_NAME ${HEADER_FILE} NAME)
    add_custom_command(
        TARGET ${PROJECT_NAME}_qc
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            ${HEADER_FILE}
            ${HEADER_OUTPUT_PATH}/${HEADER_NAME}
        COMMENT "Copying ${HEADER_NAME} to ${HEADER_OUTPUT_PATH}"
    )
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	#add_subdirectory(mtk/pcie/common/pcie-adb)
	add_subdirectory(mtk/pcie/linux/dumplog)
endif()

# 定义要复制的文件路径
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	if(NOT WAF_BINARY_COMMON_ARCH STREQUAL "arm")
		waf_binary_common_path(_DUMP_MTK_USB ${CMAKE_CURRENT_SOURCE_DIR}/mtk/usb/linux/binary_common)
		waf_binary_common_path(_DUMP_MTK_PCIE ${CMAKE_CURRENT_SOURCE_DIR}/mtk/pcie/linux/binary_common)
		set(mtk_dump_usb_path "${_DUMP_MTK_USB}/coredump/")
		set(mtk_pcie_adb_path "${_DUMP_MTK_PCIE}/pcie-adb_binary/pcie-adb")
		
		# 定义要复制到的目标目录
		set(target_dir "${CMAKE_BINARY_DIR}/release_lib/adapter_product/tools/mtk")


		# 添加自定义命令来复制文件
		add_custom_command(
			OUTPUT ${target_dir}/coredump ${target_dir}/pcie-adb
			COMMAND ${CMAKE_COMMAND} -E make_directory ${target_dir}
			COMMAND ${CMAKE_COMMAND} -E copy_directory ${mtk_dump_usb_path} ${target_dir}
			COMMAND ${CMAKE_COMMAND} -E copy ${mtk_pcie_adb_path} ${target_dir}/pcie-adb
			DEPENDS ${mtk_dump_usb_path} ${mtk_pcie_adb_path}
			COMMENT "Copying mtk mini dump tools to the build directory."
		)

		# 创建自定义目标来执行复制操作
		add_custom_target(copy_dump_tools ALL
			DEPENDS ${target_dir}/coredump
		)

		# 确保目标项目依赖于复制库文件的操作
		add_dependencies(${PROJECT_NAME}_mtk copy_dump_tools)
	endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
endif()

