cmake_minimum_required(VERSION 3.10)
project(dumplog C)

# 公共源文件
set(COMMON_SRC_FILES
    dumplog.c
)

set(CMAKE_CXX_CLANG_TIDY "")

# 生成可执行文件
add_executable(dumplog ${COMMON_SRC_FILES} )

# 链接库
target_link_libraries(dumplog)

target_compile_options(dumplog PRIVATE -w)

#############################################################################
# WAF
#############################################################################

set(CMAKE_RELEASE_DIR ${CMAKE_BINARY_DIR}/release_lib)
set(LIBRARY_OUTPUT_PATH ${CMAKE_RELEASE_DIR}/adapter_product/tools/mtk/)

add_custom_command(
    TARGET dumplog
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:dumplog> ${LIBRARY_OUTPUT_PATH}/dumplog
)