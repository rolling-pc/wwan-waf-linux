cmake_minimum_required(VERSION 3.22)

include(${CMAKE_SOURCE_DIR}/cmake/WafArch.cmake)

set(CMAKE_CXX_STANDARD 20)
project(afal_common VERSION 1.0.0 LANGUAGES CXX)
find_package(Qt5 REQUIRED COMPONENTS Core Xml SerialPort)
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RELEASE_DIR ${CMAKE_BINARY_DIR}/release_lib)
set(LIBRARY_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/adapter_product)
set(HEADER_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/adapter_product/api)  # 头文件的目标目录
set(MOC_HEADER_FILES compress.h modem.h commonprocess.h)													

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
     set(SYMLINK_PATH ${CMAKE_RELEASE_DIR}/adapter_product/lib${PROJECT_NAME}.so.${PROJECT_VERSION})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
     set(SYMLINK_PATH ${CMAKE_RELEASE_DIR}/adapter_product/${PROJECT_NAME}.dll)	
	 add_definitions(-DUNICODE -D_UNICODE)
	 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MD")
	 set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)		
	 #add_compile_definitions(_IS_WINDOWS_=1)	 
endif()

qt5_wrap_cpp(MOC_FILES ${MOC_HEADER_FILES})

set(HEADER_FILES  

)

set(build_file
proccommon.cc
commonprocess.cc
compress.cc
file.cc
#modem.h
)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    list(APPEND build_file linux/modem_linux.cc)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND build_file windows/modem.cc)
    list(APPEND build_file windows/NetAdapterIoControl.cpp)
    list(APPEND build_file windows/pch.cpp)    
endif()

# 定义头文件路径

set(build_head_path 
    ${CMAKE_CURRENT_SOURCE_DIR}/
	${CMAKE_CURRENT_SOURCE_DIR}/windows
    ${CMAKE_CURRENT_SOURCE_DIR}/../api
    ${CMAKE_SOURCE_DIR}/third_party/common/xz-5.x/src/liblzma/api
	${CMAKE_CURRENT_SOURCE_DIR}/../../adapter_os/api
	#${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/common/xz-5.x/src/liblzma/api
	#${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/common/spdlog-1.x/include
)   

include_directories(${build_head_path})

set(local_lib_path  ${CMAKE_RELEASE_DIR}/adapter_os
                    ${CMAKE_RELEASE_DIR}/adapder_product
                    ${CMAKE_RELEASE_DIR}/private_dev/
                    ${CMAKE_RELEASE_DIR}/third_party/
                   )

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_library(afal_common SHARED ${build_file} ${MOC_FILES})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_library(afal_common SHARED ${build_file} ${MOC_FILES})
endif() 

link_directories(${local_lib_path})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    target_link_libraries(afal_common ${project_sys_lib_path} udev Qt5::Core log liblzma mbim)
    add_custom_command(
          TARGET ${PROJECT_NAME}
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${LIBRARY_OUTPUT_PATH}/lib${PROJECT_NAME}.so
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(project_sys_lib_path "")
	target_compile_options(afal_common PRIVATE /MD)
    if(WAF_TARGET_ARCH STREQUAL "arm64")
        target_compile_definitions(afal_common PRIVATE WAF_SKIP_MINIDEBUGLOGGER)
        target_link_libraries(afal_common ${project_sys_lib_path} Qt5::Core log liblzma Cfgmgr32 mbim PowrProf)
    else()
        target_link_libraries(afal_common ${project_sys_lib_path} Qt5::Core MiniDebugLogger log liblzma Cfgmgr32 mbim PowrProf)
    endif()
    add_custom_command(
          TARGET ${PROJECT_NAME}
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${LIBRARY_OUTPUT_PATH}/${PROJECT_NAME}.dll
    )
endif() 


