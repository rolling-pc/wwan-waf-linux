cmake_minimum_required(VERSION 3.10)
project(pcie_adb_project C)

# 公共源文件
set(COMMON_SRC_FILES
    adb.c
    fdevent.c
    transport.c
    transport_local.c
    sockets.c
    services.c
    transport_pcie.c
)

# Linux 专用源文件
set(LINUX_SRC_FILES
    adb_auth_host.c
    adb_client.c
    commandline.c
    console.c
    file_sync_client.c
    get_my_path_linux.c
    socket_inaddr_any_server.c
    socket_loopback_client.c
    socket_loopback_server.c
    socket_network_client.c
    socket_local_client.c
    socket_local_server.c
    load_file.c
    sha.c
    sha256.c
    centraldir.c
    zipfile.c
)

# Windows 专用源文件（与 Linux 源文件相同）
set(WINDOWS_SRC_FILES ${LINUX_SRC_FILES})

# 编译选项
set(COMMON_CFLAGS
-D_XOPEN_SOURCE
-D_GNU_SOURCE
-Wall -Wno-unused-parameter -Wno-deprecated-declarations
)

# 平台相关设置
if(WIN32)
    set(PLATFORM_SRC_FILES ${WINDOWS_SRC_FILES})
    set(PLATFORM_CFLAGS ${COMMON_CFLAGS} -DADB_WINDOWS)
    set(LIBS
        -lcrypto
        -ldl -pthread -lc -lz
    )
    set(OUTPUT_NAME "pcie-adb.exe")
    set(LFLAGS "-Wl,-Map,pcie-adb.map")
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/windows)
else()
    set(PLATFORM_SRC_FILES ${LINUX_SRC_FILES})
    set(PLATFORM_CFLAGS ${COMMON_CFLAGS})
    set(LIBS
        -lcrypto
        -ldl -pthread -lc -lz
    )
    set(OUTPUT_NAME "pcie-adb")
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/linux)
endif()

set(CMAKE_CXX_CLANG_TIDY "")

# 添加编译器选项
add_compile_options(${PLATFORM_CFLAGS})

# 生成可执行文件
add_executable(${OUTPUT_NAME} ${COMMON_SRC_FILES} ${PLATFORM_SRC_FILES})

target_compile_definitions(${OUTPUT_NAME} PRIVATE ADB_NON_ANDROID=1
ALLOW_ADBD_ROOT=1
HAVE_PTHREADS
ADB_OVER_PCIE=1
DEBUG_PACKETS=1
ADB_HOST=1
DZONE
HAVE_FORKEXEC=1
HAVE_SYMLINKS
HAVE_TERMIO_H
)

# 链接库
target_link_libraries(${OUTPUT_NAME} ${LIBS})

# 设置链接器标志
if(WIN32)
    target_link_options(${OUTPUT_NAME} PRIVATE ${LFLAGS})
endif()

# 添加包含路径
target_include_directories(${OUTPUT_NAME} PRIVATE . ./include)

target_compile_options(${OUTPUT_NAME} PRIVATE -w)

#############################################################################
# WAF
#############################################################################

set(CMAKE_RELEASE_DIR ${CMAKE_BINARY_DIR}/release_lib)
set(LIBRARY_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/adapter_product/tools/mtk/)

add_custom_command(
    TARGET ${OUTPUT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${OUTPUT_NAME}> ${LIBRARY_OUTPUT_PATH}/${OUTPUT_NAME}
)