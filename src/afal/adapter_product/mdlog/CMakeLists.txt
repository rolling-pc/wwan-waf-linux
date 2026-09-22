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

project(mdlog VERSION 1.0.0 LANGUAGES CXX)
find_package(Qt5 REQUIRED COMPONENTS Core SerialPort Concurrent)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_RELEASE_DIR ${CMAKE_BINARY_DIR}/release_lib)
set(LIBRARY_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/adapter_product)
set(HEADER_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/adapter_product/api)  # 头文件的目标目录
set(SYMLINK_PATH ${CMAKE_RELEASE_DIR}/adapter_product/lib${PROJECT_NAME}.so.${PROJECT_VERSION})

set(MDLOG_MAJOR_VERSION 1)
set(MDLOG_MINOR_VERSION 0)
set(MDLOG_PATCH_VERSION 0)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(MDLOG_VERSION_STRING ${MDLOG_MAJOR_VERSION}.${MDLOG_MINOR_VERSION}.${MDLOG_PATCH_VERSION})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(MDLOG_VERSION_STRING ${MDLOG_MAJOR_VERSION}.${MDLOG_MINOR_VERSION}.${MDLOG_PATCH_VERSION}.0)
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
                "./version.h")




# 定义源码文件
set(build_file 
modem_log.cc
#common/modem_log_tools_impl.cc
common/modem_log_tools.h
${CMAKE_CURRENT_SOURCE_DIR}/../api/modem_log_interface.h
)

# 定义头文件路径,如需区分linux和windows的不同，根据实际情况进行追加
set(build_head_path 
    ${CMAKE_CURRENT_SOURCE_DIR}/../api/
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    #${CMAKE_CURRENT_SOURCE_DIR}/qc/usb/linux/
    #${CMAKE_CURRENT_SOURCE_DIR}/qc/usb/windows/
    ${CMAKE_SOURCE_DIR}/third_party/common/xz-5.x/src/liblzma/api
)

set(HEADER_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../api/modem_log_interface.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../api/common.h
)

set(MOCS_FILES ../api/modem_log_interface.h
                common/modem_log_tools_impl.h
                ../api/modem_log_interface.h
                ../common/compress.h
                common/modem_log_tools.h)

include_directories(${build_head_path})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        qt5_wrap_cpp(MOC_FILES ${MOCS_FILES})
        #         qc/usb/linux/modem_log_qc.h
        # )

        list(APPEND build_file ${CMAKE_CURRENT_SOURCE_DIR}/common/linux/modem_log_tools_impl.cc)

        set(build_file_qc
                ${build_file}
                common/diag_mdlog.cc
                qc/usb/linux/modem_log_qc.cc
                qc/usb/linux/rw135R/modem_log_qc_135R.cc
        )

        set(build_file_mtk
                ${build_file}
                mtk/pcie/linux/modem_mtk.cc
                mtk/pcie/linux/modem_log_mtk.cc
                mtk/pcie/linux/modem_log_tool_mtk.cc
                mtk/usb/linux/modem_log_mtk_usb.cc
        )

        list(APPEND build_head_path ${CMAKE_CURRENT_SOURCE_DIR}/qc/usb/linux/)

        set(CMAKE_INSTALL_RPATH "$ORIGIN/../adapter_product;$ORIGIN/../adapter_os;$ORIGIN/../../libs/qt_lib;
                                $ORIGIN/../cxx_lib")
        set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
        set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
        set(project_sys_lib_path "")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        qt5_wrap_cpp(MOC_FILES ${MOCS_FILES})
        #         qc/usb/windows/modem_log_qc.h
        # )
		add_definitions(-DUNICODE -D_UNICODE)
        list(APPEND build_head_path qc/usb/windows/)
        list(APPEND build_file ${CMAKE_CURRENT_SOURCE_DIR}/common/windows/modem_log_tools_impl.cc
                common/windows/SettingsManager.cpp                
        )

        set(build_file_qc  ${build_file}
            common/diag_mdlog.cc
            qc/usb/windows/modem_log_qc.cc
            qc/usb/windows/rw135R/modem_log_qc_135R.cc)

        set(build_file_mtk
                ${build_file}
                mtk/pcie/windows/modem_mtk.cc
                mtk/pcie/windows/modem_log_mtk.cc
                #mtk/modem_mtk.cc
                #mtk/modem_log_mtk.cc
                #mtk/modem_log_tool_mtk.cc   
        )

        set(project_sys_lib_path "")
endif()


add_library(${PROJECT_NAME}_qc SHARED ${build_file_qc} ${MOC_FILES})
target_compile_definitions(${PROJECT_NAME}_qc PRIVATE BUILD_QC MDLOG_VERSION_STRING="${MDLOG_VERSION_STRING}")
target_link_libraries(${PROJECT_NAME}_qc afal_common Qt5::SerialPort Qt5::Concurrent)
# 设置库的头文件路径
target_include_directories(${PROJECT_NAME}_qc PUBLIC
    $<BUILD_INTERFACE:${build_head_path}>
    $<INSTALL_INTERFACE:include>
)

# 设置库的输出目录
set_target_properties(${PROJECT_NAME}_qc PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH}
)

set(LIBRARY_OUTPUT_PATH_TOOL ${CMAKE_RELEASE_DIR}/adapter_product/tools/qc/)
set(ADAPTER_PRODUCT_MDLOG_PATH ${CMAKE_CURRENT_SOURCE_DIR})
# Linux aarch64 historically always used binary_common/x86 (Default.cfg / resetMbim.sh
# are arch-independent). Windows ARM import mapped aarch64 -> binary_common/arm, but
# linux/.../arm/ is only a placeholder. Prefer arch dir when the file exists, else x86.
waf_binary_common_path(_MDLOG_LINUX_ARCH ${ADAPTER_PRODUCT_MDLOG_PATH}/qc/usb/linux/binary_common)
set(_MDLOG_LINUX_X86 ${ADAPTER_PRODUCT_MDLOG_PATH}/qc/usb/linux/binary_common/x86)
foreach(_mdlog_file Default.cfg resetMbim.sh logtool)
    if(EXISTS "${_MDLOG_LINUX_ARCH}/${_mdlog_file}")
        set(_MDLOG_LINUX_PICKED "${_MDLOG_LINUX_ARCH}/${_mdlog_file}")
    else()
        set(_MDLOG_LINUX_PICKED "${_MDLOG_LINUX_X86}/${_mdlog_file}")
    endif()
    if(_mdlog_file STREQUAL "Default.cfg")
        set(LIBRARY_PATH_TOOL "${_MDLOG_LINUX_PICKED}")
    elseif(_mdlog_file STREQUAL "resetMbim.sh")
        set(LIBRARY_PATH_RESETMBIM "${_MDLOG_LINUX_PICKED}")
    else()
        set(LIBRARY_PATH_LOGTOOL "${_MDLOG_LINUX_PICKED}")
    endif()
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # 自定义拷贝库文件、头文件
	add_custom_command(
        TARGET ${PROJECT_NAME}_qc
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${LIBRARY_OUTPUT_PATH_TOOL}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}_qc> ${LIBRARY_OUTPUT_PATH}/lib${PROJECT_NAME}_qc.so
        COMMAND ${CMAKE_COMMAND} -E copy ${LIBRARY_PATH_TOOL} ${LIBRARY_OUTPUT_PATH_TOOL}/Default.cfg
        COMMAND ${CMAKE_COMMAND} -E copy ${LIBRARY_PATH_RESETMBIM} ${LIBRARY_OUTPUT_PATH_TOOL}/resetMbim.sh
        COMMAND chmod +x ${LIBRARY_OUTPUT_PATH_TOOL}/resetMbim.sh
    )

    if (CURRENT_PACKAGE_TYPE STREQUAL "deb" OR CURRENT_PACKAGE_TYPE STREQUAL "rpm")
        add_custom_command(
            TARGET ${PROJECT_NAME}_qc
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${LIBRARY_PATH_LOGTOOL} ${LIBRARY_OUTPUT_PATH_TOOL}/logtool
        )
    endif ()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
# 自定义拷贝库文件、头文件
    add_custom_command(
        TARGET ${PROJECT_NAME}_qc
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}_qc> ${LIBRARY_OUTPUT_PATH}/${PROJECT_NAME}_qc.dll
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_LINKER_FILE:${PROJECT_NAME}_qc> ${LIBRARY_OUTPUT_PATH}/${PROJECT_NAME}_qc.lib
    )
endif ()
# copy 所需头文件

file(GLOB DIAG_MDLOG_CFG
    "${CMAKE_SOURCE_DIR}/src/afal/adapter_os/osinfo/windows/binary_common/platform-tools/mb*_msg.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../adapter_os/osinfo/windows/binary_common/platform-tools/mb*_msg.cfg")
if(DIAG_MDLOG_CFG)
    list(REMOVE_DUPLICATES DIAG_MDLOG_CFG)
    add_custom_command(
        TARGET ${PROJECT_NAME}_qc
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${LIBRARY_OUTPUT_PATH_TOOL}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DIAG_MDLOG_CFG} ${LIBRARY_OUTPUT_PATH_TOOL}/
        COMMENT "Copying diag_mdlog mask cfg to ${LIBRARY_OUTPUT_PATH_TOOL}"
    )
endif()

set(DEPLOY_DIAG_MDLOG_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/qc/pcie/windows/binary_common/x86/DeployDiagMdlog")
set(DEPLOY_DIAG_MDLOG_SCRIPTS
    collect_ipc_log.sh
    ipa_log_start.bat
    ipa_log_stop_tcpdump.bat
    ipa_log_collector_without_input.bat
    secondary.bat)
if(EXISTS "${DEPLOY_DIAG_MDLOG_SRC}/collect_ipc_log.sh")
    set(DEPLOY_DIAG_MDLOG_DIR "${CMAKE_RELEASE_DIR}/adapter_product/tools/qc/DeployDiagMdlog")
    foreach(SCRIPT_NAME IN LISTS DEPLOY_DIAG_MDLOG_SCRIPTS)
        if(EXISTS "${DEPLOY_DIAG_MDLOG_SRC}/${SCRIPT_NAME}")
            add_custom_command(
                TARGET ${PROJECT_NAME}_qc
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIAG_MDLOG_DIR}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DEPLOY_DIAG_MDLOG_SRC}/${SCRIPT_NAME}"
                    "${DEPLOY_DIAG_MDLOG_DIR}/"
                COMMENT "Copying ${SCRIPT_NAME} to ${DEPLOY_DIAG_MDLOG_DIR}"
            )
        endif()
    endforeach()
endif()

foreach(HEADER_FILE ${HEADER_FILES})
    get_filename_component(HEADER_NAME ${HEADER_FILE} NAME)
    add_custom_command(
        TARGET ${PROJECT_NAME}_qc
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${HEADER_FILE}
            ${HEADER_OUTPUT_PATH}/${HEADER_NAME}
        COMMENT "Copying ${HEADER_NAME} to ${HEADER_OUTPUT_PATH}"
    )
endforeach()

if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND WAF_TARGET_ARCH STREQUAL "arm64"))
add_library(${PROJECT_NAME}_mtk SHARED ${build_file_mtk} ${MOC_FILES})
target_compile_definitions(${PROJECT_NAME}_mtk PRIVATE BUILD_MTK MDLOG_VERSION_STRING="${MDLOG_VERSION_STRING}")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
target_link_libraries(${PROJECT_NAME}_mtk afal_common Qt5::SerialPort Qt5::Concurrent)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
target_link_libraries(${PROJECT_NAME}_mtk afal_common Qt5::SerialPort Qt5::Concurrent MiniDebugLogger)
endif ()

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
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}_mtk> ${LIBRARY_OUTPUT_PATH}/lib${PROJECT_NAME}_mtk.so
)
# copy 所需头文件
foreach(HEADER_FILE ${HEADER_FILES})
    get_filename_component(HEADER_NAME ${HEADER_FILE} NAME)
    add_custom_command(
        TARGET ${PROJECT_NAME}_mtk
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${HEADER_OUTPUT_PATH}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${HEADER_FILE}
            ${HEADER_OUTPUT_PATH}/${HEADER_NAME}
        COMMENT "Copying ${HEADER_NAME} to ${HEADER_OUTPUT_PATH}"
    )
endforeach()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if (CURRENT_PACKAGE_TYPE STREQUAL "run")
		add_subdirectory(qc/usb/linux/binary_common/Fibocom_Linux_LogCapture)
	endif ()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	# QTraceCollection source is portable; build tree lives under x86/ for now.
	set(_QTRACE_COLLECTION_SRC ${CMAKE_CURRENT_SOURCE_DIR}/qc/usb/windows/binary_common/x86/QTraceCollection)
	if(EXISTS ${_QTRACE_COLLECTION_SRC}/CMakeLists.txt)
		add_subdirectory(${_QTRACE_COLLECTION_SRC} QTraceCollection)
	endif()
endif ()

# 添加客户端可执行文件
add_executable(ModemLogDemo demo.cc ${HEADER_FILES} ../api/modem_log_interface.h ../common/modem.h)
target_compile_options(ModemLogDemo PRIVATE -fPIC)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
target_link_libraries(ModemLogDemo Qt5::Core Qt5::Concurrent ${PROJECT_NAME}_qc afal_common log mbim)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(WAF_TARGET_ARCH STREQUAL "arm64")
        target_link_libraries(ModemLogDemo Qt5::Core Qt5::Concurrent ${PROJECT_NAME}_qc afal_common log mbim)
    else()
        target_link_libraries(ModemLogDemo Qt5::Core Qt5::Concurrent ${PROJECT_NAME}_qc afal_common log mbim MiniDebugLogger)
    endif()
endif ()