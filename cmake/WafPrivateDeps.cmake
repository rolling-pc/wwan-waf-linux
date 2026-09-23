# =============================================================================
# WafPrivateDeps.cmake — 接入预编译的私有库（private_dev / private_func）
#
# 背景：private_dev / private_func 的源码不随本仓库发布，只提供编译好的 .so。
#       本文件把这些 .so 以 IMPORTED target 的形式接入构建，使本仓库能完整
#       编译出两个服务并打包 deb。
#
# 目录约定：prebuilt/linux/<arch>/
#   libs/libprivate_dev.so      + include/private_dev/WwanDevSvc.hpp
#   libs/libprivate_func.so     + include/private_func/wwan_func_svc.hpp
#   manifest.json               版本 + sha256（构建时校验，防止用错版本）
#
# 注意：WAF_PRIVATE_LINK_* 是私有库的"传递依赖"。私有库不是 CMake target 时，
#       这些依赖不会自动进入服务 exe 的链接命令，缺了会报 undefined reference。
# =============================================================================

if(NOT DEFINED WAF_PREBUILT_DIR OR WAF_PREBUILT_DIR STREQUAL "")
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(_waf_prebuilt_arch "aarch64")
    else()
        set(_waf_prebuilt_arch "x86_64")
    endif()
    set(WAF_PREBUILT_DIR "${CMAKE_SOURCE_DIR}/prebuilt/linux/${_waf_prebuilt_arch}")
endif()
get_filename_component(WAF_PREBUILT_DIR "${WAF_PREBUILT_DIR}" ABSOLUTE)

# 私有库的传递依赖（对齐各自原有的 target_link_libraries）
set(WAF_PRIVATE_LINK_private_dev
    Qt5::Xml Qt5::Core Qt5::Network log devicemonitor osinfo)
set(WAF_PRIVATE_LINK_private_func
    Qt5::Xml Qt5::Core Qt5::Network QuaZip mbim log osinfo display power
    wwan_dev_lib curl)

if(NOT COMMAND waf_import_private_lib)

    function(waf_private_check_manifest name)
        set(_manifest "${WAF_PREBUILT_DIR}/manifest.json")
        if(NOT EXISTS "${_manifest}")
            message(FATAL_ERROR
                "找不到预编译私有库清单: ${_manifest}\n"
                "请确认 prebuilt/linux/<arch>/ 下已放置 libs / include / manifest.json")
        endif()
        file(READ "${_manifest}" _json)
        string(JSON _sha ERROR_VARIABLE _err GET "${_json}" "libs" "${name}" "sha256")
        if(_err)
            message(FATAL_ERROR "manifest.json 中缺少 ${name} 的 sha256")
        endif()
        set(_so "${WAF_PREBUILT_DIR}/libs/lib${name}.so")
        if(NOT EXISTS "${_so}")
            message(FATAL_ERROR "缺少预编译库: ${_so}")
        endif()
        file(SHA256 "${_so}" _actual)
        if(NOT _actual STREQUAL _sha)
            message(FATAL_ERROR
                "lib${name}.so 校验失败\n  期望 sha256: ${_sha}\n  实际 sha256: ${_actual}")
        endif()
    endfunction()

    # 把预编译库与接口头拷到 release_lib/<name>/，位置与"源码编译产物"完全一致，
    # 因此上游的 include 路径、link_directories、RPATH、打包脚本都不用改。
    function(waf_import_private_lib name)
        waf_private_check_manifest(${name})

        set(_dst "${CMAKE_BINARY_DIR}/release_lib/${name}")
        file(MAKE_DIRECTORY "${_dst}/api")
        file(COPY "${WAF_PREBUILT_DIR}/libs/lib${name}.so" DESTINATION "${_dst}")
        if(EXISTS "${WAF_PREBUILT_DIR}/include/${name}")
            file(COPY "${WAF_PREBUILT_DIR}/include/${name}/" DESTINATION "${_dst}/api")
        endif()

        if(NOT TARGET ${name})
            add_library(${name} SHARED IMPORTED GLOBAL)
            set_target_properties(${name} PROPERTIES
                IMPORTED_LOCATION             "${_dst}/lib${name}.so"
                INTERFACE_INCLUDE_DIRECTORIES "${_dst}/api")
        endif()

        message(STATUS "WafPrivateDeps: 已接入预编译私有库 '${name}' (${WAF_PREBUILT_DIR})")
    endfunction()

endif()
