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

file(TIMESTAMP BUILD_START_TIME "%s" UTC)
project(wwanappfreamwork VERSION 1.0.0)

include(${CMAKE_SOURCE_DIR}/cmake/WafArch.cmake)

# set(CMAKE_CXX_COMPILER "/usr/bin/g++")
# enable clang check
# 用户可缓存修改的版本组件
#set(WWAN_DEV_MAJOR_VERSION ${PROJECT_VERSION_MAJOR} CACHE STRING "Major version")
#set(WWAN_DEV_MINOR_VERSION ${PROJECT_VERSION_MINOR} CACHE STRING "Minor version")
#set(WWAN_DEV_PATCH_VERSION ${PROJECT_VERSION_PATCH} CACHE STRING "Patch version")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(WWAN_DEV_MAJOR_VERSION 1)
    set(WWAN_DEV_MINOR_VERSION 0)
    set(WWAN_DEV_PATCH_VERSION 20)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(WWAN_DEV_MAJOR_VERSION 1)
    set(WWAN_DEV_MINOR_VERSION 0)
    set(WWAN_DEV_PATCH_VERSION 39)
endif()

# 构造完整版本字符串
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(WWAN_DEV_VERSION_STRING ${WWAN_DEV_MAJOR_VERSION}.${WWAN_DEV_MINOR_VERSION}.${WWAN_DEV_PATCH_VERSION})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(WWAN_DEV_VERSION_STRING ${WWAN_DEV_MAJOR_VERSION}.${WWAN_DEV_MINOR_VERSION}.${WWAN_DEV_PATCH_VERSION}.0)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_BUILD_TYPE Release)
#set(CMAKE_BUILD_TYPE Debug)

if (MSVC)
    add_compile_options("$<$<CONFIG:Debug>:/Zi>")
    add_compile_options("$<$<CONFIG:Release>:/Zi>")
    add_compile_options("$<$<CONFIG:RelWithDebInfo>:/Zi>")
    add_compile_options("$<$<CONFIG:MinSizeRel>:/Zi>")

    add_link_options("$<$<CONFIG:Debug>:/DEBUG>")
    add_link_options("$<$<CONFIG:Release>:/DEBUG>")
    add_link_options("$<$<CONFIG:RelWithDebInfo>:/DEBUG>")
    add_link_options("$<$<CONFIG:MinSizeRel>:/DEBUG>")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_definitions(_IS_DEBUG_=1)
		message("current build is debug")
	elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -s")
	endif()

else()
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    add_definitions(-DUNICODE -D_UNICODE)
    if(INSTALL_TOOLS_ENABLE)
        add_compile_definitions(TOOLService)
        message("Windows: TOOLService is enabled")
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		message("current build is debug")
        add_compile_definitions(_IS_DEBUG_=1)
	elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
	endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(CLANG_TIDY_BIN NAMES "clang-tidy")

    # if(CLANG_TIDY_BIN)
    #     set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_BIN}")
    # endif()
endif()

# 增加以下逻辑，防止报警告
if(INSTALL_TOOLS_ENABLE)
    set(FUNCTION_MA_ENABLE "")
    message("Ignoring unused variables for tools installation.")
elseif(PROJECT_UNIT_TEST)
    set(CURRENT_PACKAGE_TYPE "")
    message("Ignoring unused variables for tools installation.")
elseif(INSTALL_LIB_ENABLE)
    set(CURRENT_PACKAGE_TYPE "")
    set(FUNCTION_MA_ENABLE "")
    message("Ignoring unused variables for tools installation.")
elseif(INSTALL_CLI_ENABLE)
    set(COMPILER_CHIP_PLATFORM "")
    set(COMPILER_OS_PLATFORM "")
    set(FUNCTION_MA_ENABLE "")
    message("Ignoring unused variables for tools installation.")
else()
    message("No specific installation type is enabled.")
endif()

# include(${CMAKE_SOURCE_DIR}/cmake/options.cmake)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_definitions(_IS_LINUX_=1)
    set(DEFAULT_OS_PREFIX "/opt/waf")
    message("This is Linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_compile_definitions(_IS_WINDOWS_=1)
    set(DEFAULT_OS_PREFIX "C:/waf")
    message("This is Windows")
	add_compile_definitions(_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR) 
else()
    message(FATAL_ERROR "Unsupported OS: ${CMAKE_SYSTEM_NAME}")
endif()

if(DEFINED CURRENT_PACKAGE_TYPE AND CURRENT_PACKAGE_TYPE STREQUAL "run" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_definitions(OS_PREFIX="/usr/local/opt/waf")
else()
    add_compile_definitions(OS_PREFIX="${DEFAULT_OS_PREFIX}")
endif()

message("current system is ${CMAKE_SYSTEM_NAME}")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(INSTALL_TOOLS_ENABLE)
        add_compile_definitions(TOOLService)
        message("Linux: TOOLService is enabled")
    endif()
    if(INSTALL_SERVICE_ENABLE AND ((CURRENT_PACKAGE_TYPE STREQUAL "deb") OR (CURRENT_PACKAGE_TYPE STREQUAL "rpm")))
        add_compile_definitions(LINUXService)
        add_compile_definitions(TOOLService)
        message("Linux: LINUXService is enabled")
        message("Linux: TOOLService is enabled for service/toolkits communication")
    elseif(PROJECT_UNIT_TEST)
        add_compile_definitions(TOOLService)
        message("Linux: TOOLService is enabled for unit test")
    endif()
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX ${DEFAULT_INSTALL_PREFIX} CACHE PATH "Default installation path" FORCE)
endif()
message(STATUS "Installation prefix: ${CMAKE_INSTALL_PREFIX}")

# get current os type

# Based on different configuration items，Add directory compilation
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	add_subdirectory(third_party)
	add_subdirectory(src)
	add_subdirectory(lib)
else()
	add_subdirectory(third_party)
	add_subdirectory(src)
	add_subdirectory(lib)
	#add_subdirectory(WinRTDemo)
endif()

if(PROJECT_UNIT_TEST)
    enable_testing()
    include(GoogleTest)
    add_subdirectory(test)
endif()

string(FIND "${CURRENT_PROJECT}" "," COMMA_POSITION)

set(PROJECT_LIST,"")
if(COMMA_POSITION GREATER -1)
    string(REPLACE "," ";" PROJECT_LIST "${CURRENT_PROJECT}")
    message(STATUS "Project list: ${PROJECT_LIST}")
    foreach(PROJECT ${PROJECT_LIST})
        message(STATUS "Processing project: ${PROJECT}")
    endforeach()
else()
    set(PROJECT_LIST "${CURRENT_PROJECT}")
    message(STATUS "Single project: ${PROJECT_LIST}")
    if("${CURRENT_PROJECT}" STREQUAL "rw135")
        message("current project ${CURRENT_PROJECT}")
    endif()
endif()


string(TOLOWER "${CURRENT_PROJECT}" lower_project)
set(PROJECT_COMP_TYPE "rolling")
# if(lower_project MATCHES "rw")
#     set(PROJECT_COMP_TYPE "rolling")
# elseif(lower_project MATCHES "fm")
#     set(PROJECT_COMP_TYPE "fibocom")
# endif()

message(STATUS "PROJECT_COMP_TYPE: ${PROJECT_COMP_TYPE}")

if("${CURRENT_OEM}" STREQUAL "hp")
    message("current project ${CURRENT_OEM}")
endif()

function(run_install_script target_name package_type)
    install(CODE "
        MESSAGE(\"Start installing ${target_name} package ${package_type}.\")
        execute_process(COMMAND bash ${PROJECT_SOURCE_DIR}/script/linux/make_install/install ${target_name} ${package_type}
                        RESULT_VARIABLE result)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR \"Install script for ${target_name} failed with exit code ${result}\")
        endif()
        MESSAGE(\"Successfully installed ${target_name} package ${package_type}.\")
    ")
endfunction()

set(install_dir "${PROJECT_BINARY_DIR}/release")

function(service_linux_install_files)
    install(CODE "MESSAGE(\"service install files.\")")
    install(DIRECTORY "${install_dir}/opt/waf" FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ DESTINATION "${CMAKE_INSTALL_PREFIX}/opt")
    install(DIRECTORY "${install_dir}/lib/systemd/system/"  
        FILE_PERMISSIONS    
        OWNER_EXECUTE OWNER_WRITE OWNER_READ
        GROUP_EXECUTE GROUP_READ 
        WORLD_EXECUTE WORLD_READ
        DESTINATION
        "${CMAKE_INSTALL_PREFIX}/lib/systemd/system")
		
	install(DIRECTORY "${install_dir}/usr/lib/udev/"  
				FILE_PERMISSIONS    
				OWNER_EXECUTE OWNER_WRITE OWNER_READ
				GROUP_EXECUTE GROUP_READ 
				WORLD_EXECUTE WORLD_READ
				DESTINATION
				"${CMAKE_INSTALL_PREFIX}/usr/lib/udev/")
	install(DIRECTORY "${install_dir}/usr/lib/udev/rules.d/"  
				FILE_PERMISSIONS    
				OWNER_EXECUTE OWNER_WRITE OWNER_READ
				GROUP_EXECUTE GROUP_READ 
				WORLD_EXECUTE WORLD_READ
				DESTINATION
				"${CMAKE_INSTALL_PREFIX}/usr/lib/udev/rules.d")
endfunction()

function(service_run_install_files)
    install(CODE "MESSAGE(\"service install files.\")")
    install(DIRECTORY "${install_dir}/usr/local/opt/waf" FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ DESTINATION ".")

endfunction()

# function(tools_deb_install_files)
#     install(CODE "MESSAGE(\"service install files.\")")
#     install(DIRECTORY "${install_dir}/opt/rolling" FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ DESTINATION "${CMAKE_INSTALL_PREFIX}/opt")
#     install(DIRECTORY "${install_dir}/opt/rolling/tools/tool_kits_service/tool_kits.ico" FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ DESTINATION "${CMAKE_INSTALL_PREFIX}/opt/rolling/tools/tool_kits_service/")
#     install(DIRECTORY "${install_dir}/usr/share/applications/tool_kits.desktop" FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ DESTINATION "${CMAKE_INSTALL_PREFIX}/usr/share/applications/")
# endfunction()

function(tools_deb_install_files)
    install(CODE "MESSAGE(\"service install files.\")")

    # 安装目录
    install(DIRECTORY "${install_dir}/opt/waf"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/opt"
            FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ)

    # 安装工具图标并设置权限为755
    # install(FILES "${install_dir}/opt/rolling/tools/tool_kits_service/tool_kits.ico"
    #         DESTINATION "${CMAKE_INSTALL_PREFIX}/opt/rolling/tools/tool_kits_service/"
    #         PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    install(FILES "${install_dir}/opt/waf/tools/tool_kits_service/rolling.png"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/opt/waf/tools/tool_kits_service/"
            PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

    # 安装桌面文件并设置权限为755
    install(FILES "${install_dir}/usr/share/applications/tool_kits.desktop"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/usr/share/applications/"
            PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endfunction()



function(tools_cli_install_files)
    install(CODE "MESSAGE(\"service install files.\")")
    install(DIRECTORY "${install_dir}/opt/waf" FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ DESTINATION "${CMAKE_INSTALL_PREFIX}/opt")
endfunction()

function(tool_cli_run_install_files)
    install(CODE "MESSAGE(\"service install files.\")")
    install(DIRECTORY "${install_dir}/usr/local/opt/waf" FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ DESTINATION ".")

endfunction()



if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(NOT PROJECT_UNIT_TEST)
        # 根据不同的安装选项进行处理
        if(INSTALL_SERVICE_ENABLE)
            if((${CURRENT_PACKAGE_TYPE} STREQUAL "deb") OR (${CURRENT_PACKAGE_TYPE} STREQUAL "rpm"))
                run_install_script("service" "deb")
                #add_compile_definitions(LINUXService)
                #message("Linux: LINUXService is enabled")
                service_linux_install_files()
            elseif(${CURRENT_PACKAGE_TYPE} STREQUAL "run")
                run_install_script("service_tools" "run")
                service_run_install_files()
            endif()
            include(${CMAKE_SOURCE_DIR}/script/linux/cpack_cfg/service_CPackConfig.cmake)
        elseif(INSTALL_TOOLS_ENABLE)
            if((${CURRENT_PACKAGE_TYPE} STREQUAL "deb") OR (${CURRENT_PACKAGE_TYPE} STREQUAL "rpm"))
                run_install_script("tools" "deb")
                tools_deb_install_files()
            elseif(${CURRENT_PACKAGE_TYPE} STREQUAL "run")
                run_install_script("tools" "run")
                tool_cli_run_install_files()
            endif()
            include(${CMAKE_SOURCE_DIR}/script/linux/cpack_cfg/tool_CPackConfig.cmake)
        elseif(INSTALL_CLI_ENABLE)
            if((${CURRENT_PACKAGE_TYPE} STREQUAL "deb") OR (${CURRENT_PACKAGE_TYPE} STREQUAL "rpm"))
                run_install_script("cli" "deb")
                tools_cli_install_files()
            elseif(${CURRENT_PACKAGE_TYPE} STREQUAL "run")
                run_install_script("cli" "run")
                tool_cli_run_install_files()
            endif()
            include(${CMAKE_SOURCE_DIR}/script/linux/cpack_cfg/fibcli_CPackConfig.cmake)
        else()
            message(STATUS "Unknown package type, build service")
        endif()
    endif()
endif() 

set(BUILD_DIR ${CMAKE_BINARY_DIR})

set(PARENT_DIR "${CMAKE_BINARY_DIR}/../")

add_custom_target(copy_compile_commands ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${BUILD_DIR}/compile_commands.json
        ${PARENT_DIR}/compile_commands.json
    COMMENT "Copying compile_commands.json to the parent directory"
)
# add_custom_target(
#     run_build_ui ALL
#     DEPENDS ${CMAKE_BINARY_DIR}/dummy_marker
#     COMMENT "Running build_ui script target"
# )

# add_custom_target(print_build_time ALL
#     COMMAND ${CMAKE_COMMAND} -E echo "Calculating build time..."
#     COMMAND ${CMAKE_COMMAND} -E echo "Build End Time: $$(date +%s)"
#     COMMAND ${CMAKE_COMMAND} -E echo "Build Time Elapsed: $$(expr $$(date +%s) - ${BUILD_START_TIME}) seconds"
# )

# add_dependencies(print_build_time my_target)