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
include(${CMAKE_SOURCE_DIR}/cmake/WafArch.cmake)
project(flash VERSION 1.0.0 LANGUAGES CXX)
find_package(Qt5 REQUIRED COMPONENTS Core Xml SerialPort)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	add_compile_definitions(Q_OS_LINUX)
	set(LIB_EXT "so")
	#set(SYMLINK_PATH ${CMAKE_RELEASE_DIR}/adapter_product/lib${PROJECT_NAME}.so.${PROJECT_VERSION}) //no reference below
	set(FLASH_VERSION_STRING ${FLASH_MAJOR_VERSION}.${FLASH_MINOR_VERSION}.${FLASH_PATCH_VERSION})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(CMAKE_BUILD_TYPE Release)
	set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")  
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG")	
	add_compile_definitions(Q_OS_WIN)
	add_compile_definitions(OSINFO_EXPORTS)
	set(LIB_EXT "lib")
	set(FLASH_VERSION_STRING ${FLASH_MAJOR_VERSION}.${FLASH_MINOR_VERSION}.${FLASH_PATCH_VERSION}.0)
endif()

qt5_wrap_cpp(MOC_FILES_QC ../api/firmware_flash_interface.h firmware_flash.h qc/usb/firmware_flash_qc.h qc/pcie/firmware_flash_qc_pcie.h) #解决include moc文件问题
qt5_wrap_cpp(MOC_FILES_MTK ../api/firmware_flash_interface.h firmware_flash.h mtk/pcie/flash_tools_mtk_pcie.h) #解决include moc文件问题

set(CMAKE_RELEASE_DIR ${CMAKE_BINARY_DIR}/release_lib)
set(LIBRARY_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/adapter_product)
set(HEADER_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/adapter_product/api)  # 头文件的目标目录

set(FLASH_MAJOR_VERSION 1)
set(FLASH_MINOR_VERSION 0)
set(FLASH_PATCH_VERSION 0)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
                "./version.h")

# 定义源码文件
set(build_file
    firmware_flash.cc 
    common/flash_tools.cc
    # ../common/modem.h
    # ../api/firmware_flash_interface.h
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(build_file_mtk
		${build_file}
		# mtk/mtk_common.cc
		mtk/usb/firmware_flash_mtk_usb.cc
		mtk/usb/flash_tools_mtk_usb.cc
		mtk/pcie/firmware_flash_mtk_pcie.cc
		mtk/pcie/flash_tools_mtk_pcie.cc
	)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(build_file_mtk
		${build_file}
		mtk/pcie/firmware_flash_mtk_pcie.cc
		mtk/pcie/flash_tools_mtk_pcie.cc
        mtk/pcie/windows/5g_download_tool.cpp
        mtk/pcie/windows/flash_dll.cpp
	)
endif()

set(build_file_qc
    ${build_file}
    qc/qc_common.cc
    qc/usb/firmware_flash_qc.cc
    qc/pcie/firmware_flash_qc_pcie.cc
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND build_file_qc
        qc/usb/linux/firmware_flash_qc_linux.cc
        qc/pcie/linux/firmware_flash_qc_pcie_linux.cc
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND build_file_qc
        qc/usb/windows/firmware_flash_qc_windows.cc
        qc/pcie/windows/firmware_flash_qc_pcie_windows.cc
    )
endif()

set(HEADER_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../api/firmware_flash_interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../api/common.h
)

set(ADAPTER_PRODUCT_FLASH_PATH ${CMAKE_CURRENT_SOURCE_DIR})
# 定义头文件路径
set(build_head_path 
    ${PROJECT_BINARY_DIR}/
    ${ADAPTER_PRODUCT_FLASH_PATH}/../api/
    ${ADAPTER_PRODUCT_FLASH_PATH}/../common
    ${ADAPTER_PRODUCT_FLASH_PATH}/qc/usb
    ${ADAPTER_PRODUCT_FLASH_PATH}/qc/pcie
    ${ADAPTER_PRODUCT_FLASH_PATH}/mtk/pcie
    ${ADAPTER_PRODUCT_FLASH_PATH}/mtk/usb
    ${ADAPTER_PRODUCT_FLASH_PATH}/
    ${ADAPTER_PRODUCT_FLASH_PATH}/common
    ${CMAKE_SOURCE_DIR}/third_party/common/spdlog-1.x/include
	${CMAKE_SOURCE_DIR}/third_party/common/xz-5.x/src/liblzma/api
	#${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/common/spdlog-1.x/include // local test
    ${ADAPTER_PRODUCT_FLASH_PATH}/../../adapter_os/api/
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Linux historically always used binary_common/x86. FLASHLIB headers are
    # arch-independent; linux/.../arm/ is only a placeholder. Do not map
    # aarch64 onto arm/ or the include of Flash.Mode.API.h fails.
    list(APPEND build_head_path
        ${ADAPTER_PRODUCT_FLASH_PATH}/mtk/usb/linux/binary_common/x86/FLASHLIB_EXE_v6.2404.00.000/Inc/
    )
	set(CMAKE_INSTALL_RPATH "$ORIGIN/../adapter_product;$ORIGIN/../adapter_os;$ORIGIN/../../libs/qt_lib;
                            $ORIGIN/../cxx_lib")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(project_sys_lib_path "")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND build_head_path
        ${ADAPTER_PRODUCT_FLASH_PATH}/mtk/pcie/windows
    )
	set(project_sys_lib_path "")
    link_directories(${CMAKE_RELEASE_DIR}/adapter_os ${CMAKE_RELEASE_DIR}/adapter_product)
endif()

set(local_lib_path  ${CMAKE_RELEASE_DIR}/adapter_os
                    ${CMAKE_RELEASE_DIR}/adapter_product 
                    ${ADAPTER_PRODUCT_FLASH_PATH}/mtk/usb/linux/binary_common/x86/FLASHLIB_EXE_v6.2404.00.000/Lib/ 
					${ADAPTER_PRODUCT_FLASH_PATH}/mtk/pcie/windows/binary_common/${WAF_BINARY_COMMON_ARCH}/FLASHLIB_EXE_v5.2246.00.000/Lib
                   )
message(${local_lib_path})  

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(CMAKE_INSTALL_RPATH "$ORIGIN/../../libs/adapter_product;$ORIGIN/../../libs/adapter_os;$ORIGIN/../../libs/qt_lib;
							$ORIGIN/../../libs/cxx_lib")
	set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
	set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
	set(project_sys_lib_path ""
							)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(project_sys_lib_path "")
    link_directories(${CMAKE_RELEASE_DIR}/adapter_os ${CMAKE_RELEASE_DIR}/adapter_product)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
add_library(${PROJECT_NAME}_mtk SHARED ${build_file_mtk} ${MOC_FILES_MTK})
add_library(${PROJECT_NAME}_qc SHARED ${build_file_qc} ${MOC_FILES_QC})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
add_library(${PROJECT_NAME}_mtk SHARED ${build_file_mtk} ${MOC_FILES_MTK})
add_library(${PROJECT_NAME}_qc SHARED ${build_file_qc} ${MOC_FILES_QC})
endif()
target_compile_definitions(${PROJECT_NAME}_mtk PRIVATE BUILD_MTK FLASH_VERSION_STRING="${FLASH_VERSION_STRING}")
target_compile_definitions(${PROJECT_NAME}_qc PRIVATE BUILD_QC FLASH_VERSION_STRING="${FLASH_VERSION_STRING}")

include_directories(${build_head_path})
link_directories(${local_lib_path})


waf_binary_common_path(_FLASH_WIN_LIB ${ADAPTER_PRODUCT_FLASH_PATH}/mtk/pcie/windows/binary_common)
set(LIBRARY_PATH_WINDOWS ${_FLASH_WIN_LIB}/FLASHLIB_EXE_v5.2246.00.000/Lib)
set(LIBRARY_PATH_LINUX ${ADAPTER_PRODUCT_FLASH_PATH}/mtk/usb/linux/binary_common/x86/FLASHLIB_EXE_v6.2404.00.000/Lib)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_library(FLASH_LIB flash.1.0.0 PATHS ${LIBRARY_PATH_LINUX})
	link_directories(${LIBRARY_PATH_LINUX})
	if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
		target_link_libraries(${PROJECT_NAME}_mtk Qt5::Xml Qt5::SerialPort afal_common ${FLASH_LIB}) #${CMAKE_RELEASE_DIR}/adapter_product/libflash.so)
	elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
		target_link_libraries(${PROJECT_NAME}_mtk Qt5::Xml Qt5::SerialPort afal_common)
	endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	# Windows ARM64 has no MTK flash.dll; same policy as Linux aarch64.
	if(NOT WAF_TARGET_ARCH STREQUAL "arm64")
		set(FLASH_LIB "${LIBRARY_PATH_WINDOWS}/flash.dll")
		link_directories(${LIBRARY_PATH_WINDOWS})
	endif()
	target_link_libraries(${PROJECT_NAME}_mtk Qt5::Xml Qt5::SerialPort afal_common osinfo) 
endif()

target_link_libraries(${PROJECT_NAME}_qc  Qt5::Xml Qt5::SerialPort afal_common osinfo)

# 设置库的头文件路径
target_include_directories(${PROJECT_NAME}_mtk PUBLIC
    $<BUILD_INTERFACE:${build_head_path}>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(${PROJECT_NAME}_qc PUBLIC
    $<BUILD_INTERFACE:${build_head_path}>
    $<INSTALL_INTERFACE:include>
)

# 设置库的输出目录
set_target_properties(${PROJECT_NAME}_mtk PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH}
)

set_target_properties(${PROJECT_NAME}_qc PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH}
)


# 自定义拷贝库文件、头文件
add_custom_command(
    TARGET ${PROJECT_NAME}_mtk
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}_mtk> ${LIBRARY_OUTPUT_PATH}/lib${PROJECT_NAME}_mtk.${LIB_EXT}
)

add_custom_command(
    TARGET ${PROJECT_NAME}_qc
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}_qc> ${LIBRARY_OUTPUT_PATH}/lib${PROJECT_NAME}_qc.${LIB_EXT}
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

# 定义要复制的文件路径
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(flash_library_path "${LIBRARY_PATH_LINUX}/libflash.1.0.0.so")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(flash_library_path "${LIBRARY_PATH_WINDOWS}/flash.dll")
endif()

# 定义要复制到的目标目录
set(target_dir "${CMAKE_BINARY_DIR}/release_lib/adapter_product")

# 添加自定义命令来复制文件
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
		add_custom_command(
			OUTPUT ${target_dir}/libflash.1.0.0.so
			COMMAND ${CMAKE_COMMAND} -E make_directory ${target_dir}
			COMMAND ${CMAKE_COMMAND} -E copy ${flash_library_path} ${target_dir}/libflash.1.0.0.so
			DEPENDS ${flash_library_path}
			COMMENT "Copying libflash.1.0.0.so to the build directory."
		)
		
		# 创建自定义目标来执行复制操作
		add_custom_target(copy_flash_library ALL
			DEPENDS ${target_dir}/libflash.1.0.0.so
		)

		# 确保目标项目依赖于复制库文件的操作
		add_dependencies(${PROJECT_NAME}_mtk copy_flash_library)
	endif()

elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	if(WAF_TARGET_ARCH STREQUAL "arm64")
		message(STATUS "Windows ARM64: MTK flash.dll not used on this platform.")
	else()
		if(EXISTS "${flash_library_path}")
			add_custom_command(
				OUTPUT ${target_dir}/flash.dll
				COMMAND ${CMAKE_COMMAND} -E make_directory ${target_dir}
				COMMAND ${CMAKE_COMMAND} -E copy ${flash_library_path} ${target_dir}/flash.dll
				DEPENDS ${flash_library_path}
				COMMENT "Copying flash.dll to the build directory."
			)

			add_custom_target(copy_flash_library ALL
				DEPENDS ${target_dir}/flash.dll
			)

			add_dependencies(${PROJECT_NAME}_mtk copy_flash_library)
		else()
			message(WARNING "flash.dll not found at ${flash_library_path}; "
				"MTK flash vendor lib skipped for WAF_TARGET_ARCH=${WAF_TARGET_ARCH}.")
		endif()
	endif()

	add_subdirectory(qc/usb/windows/binary_common/QSaharaServer)
	# fh_loader is shipped from FlashTool binary_common; keep source for future updates.
	# add_subdirectory(qc/usb/windows/binary_common/fh_loader)

endif()
