# WafArch.cmake — cross-OS / cross-architecture helpers for binary_common layout.
#
# binary_common convention (adapter_product):
#   x86/  — Windows x64 and Linux x86_64 prebuilt vendor tools
#   arm/  — Windows ARM64 and Linux aarch64 prebuilt vendor tools
#
# Cache variable WAF_TARGET_ARCH (optional):
#   Windows: x64 | arm64
#   Linux:   x64 | aarch64  (x64 maps to binary_common/x86)
#
# Derived:
#   WAF_BINARY_COMMON_ARCH — subdirectory name: x86 or arm

if(DEFINED WAF_ARCH_INCLUDED)
    return()
endif()
set(WAF_ARCH_INCLUDED TRUE)

if(NOT DEFINED WAF_TARGET_ARCH OR WAF_TARGET_ARCH STREQUAL "")
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
            set(WAF_TARGET_ARCH "arm64")
        else()
            set(WAF_TARGET_ARCH "x64")
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
            set(WAF_TARGET_ARCH "aarch64")
        else()
            set(WAF_TARGET_ARCH "x64")
        endif()
    else()
        set(WAF_TARGET_ARCH "x64")
    endif()
endif()

if(WAF_TARGET_ARCH STREQUAL "arm64" OR WAF_TARGET_ARCH STREQUAL "aarch64")
    set(WAF_BINARY_COMMON_ARCH "arm")
else()
    set(WAF_BINARY_COMMON_ARCH "x86")
endif()

# waf_binary_common_path(OUT base_dir) -> base_dir/x86|arm
function(waf_binary_common_path OUT_VAR BASE_DIR)
    string(STRIP "${BASE_DIR}" BASE_DIR)
    while(BASE_DIR MATCHES "/$")
        string(LENGTH "${BASE_DIR}" _len)
        math(EXPR _len "${_len} - 1")
        string(SUBSTRING "${BASE_DIR}" 0 ${_len} BASE_DIR)
    endwhile()
    set(${OUT_VAR} "${BASE_DIR}/${WAF_BINARY_COMMON_ARCH}" PARENT_SCOPE)
endfunction()

# Prefer arch-specific subdir on ARM64; keep flat BASE_DIR for x64 (legacy layout).
function(waf_resolve_binary_common_dir OUT_VAR BASE_DIR)
    if(WAF_TARGET_ARCH STREQUAL "arm64")
        set(_candidate "${BASE_DIR}/${WAF_BINARY_COMMON_ARCH}")
        if(EXISTS "${_candidate}")
            set(${OUT_VAR} "${_candidate}" PARENT_SCOPE)
        else()
            set(${OUT_VAR} "${BASE_DIR}" PARENT_SCOPE)
        endif()
    else()
        set(${OUT_VAR} "${BASE_DIR}" PARENT_SCOPE)
    endif()
endfunction()

message(STATUS "WafArch: WAF_TARGET_ARCH=${WAF_TARGET_ARCH}, WAF_BINARY_COMMON_ARCH=${WAF_BINARY_COMMON_ARCH}")
