"""Shared architecture resolution for WAF build scripts (Windows / Linux)."""

import os

# Normalized target arch -> binary_common subdirectory name
BINARY_COMMON_ARCH = {
    "x64": "x86",
    "arm64": "arm",
    "aarch64": "arm",
}

# Windows build profiles keyed by WAF_TARGET_ARCH
WINDOWS_PROFILES = {
    "x64": {
        "qt_prefix": "c:/Qt/5.15.2/msvc2019_64",
        "zlib_library": "${sourceDir}/third_party/common/quazip-master/zlib_windows/zlib.lib",
        "zlib_include": "${sourceDir}/third_party/common/quazip-master/zlib_windows",
        "zlib_dll_subdir": "third_party/common/quazip-master/zlib_windows",
        "openssl_dlls": ["libcrypto-1_1-x64.dll", "libssl-1_1-x64.dll"],
        "openssl_lib_relpath": "src/afal/adapter_os/openssl_lib/windows",
        "curl_dlls": ["libcurl.dll"],
        "curl_lib_relpath": "src/afal/adapter_os/curl_lib/windows",
        "vcruntime_relpath": "third_party/windows/VCRuntime/x86",
        "sevenzip_host_relpath": "third_party/windows/7zip/x86",
        "sevenzip_pack_relpath": "third_party/windows/7zip/x86",
        "skip_openssl_packaging": False,
        "skip_zlib_packaging": False,
        "package_zlib_runtime_dll": True,
        "msi_suffix": "x64",
        "wix_platform": "x64",
        "wix_win64": True,
    },
    "arm64": {
        "qt_prefix": "c:/Qt/5.15.2/msvc2019_arm64",
        "zlib_library": "${sourceDir}/third_party/common/quazip-master/zlib_windows_arm64/zlibstatic.lib",
        "zlib_include": "${sourceDir}/third_party/common/quazip-master/zlib_windows_arm64",
        "zlib_dll_subdir": "third_party/common/quazip-master/zlib_windows_arm64",
        "openssl_dlls": ["libcrypto-1_1-arm64.dll", "libssl-1_1-arm64.dll"],
        "openssl_lib_relpath": "src/afal/adapter_os/openssl_lib/windows/arm64",
        "curl_dlls": ["libcurl.dll"],
        "curl_lib_relpath": "src/afal/adapter_os/curl_lib/windows/arm64",
        "vcruntime_relpath": "third_party/windows/VCRuntime/arm",
        "sevenzip_host_relpath": "third_party/windows/7zip/x86",
        "sevenzip_pack_relpath": "third_party/windows/7zip/arm",
        "skip_openssl_packaging": False,
        "skip_zlib_packaging": False,
        "package_zlib_runtime_dll": False,
        "msi_suffix": "arm64",
        "wix_platform": "arm64",
        "wix_win64": False,
    },
}


def _normalize_path(path):
    return path.replace("\\", "/")


def _apply_env_overrides(profiles):
    host = os.environ.get("WAF_SEVENZIP_HOST")
    if host:
        host = _normalize_path(host)
        for arch in profiles:
            profiles[arch]["sevenzip_host_relpath"] = host

    for arch in profiles:
        env_key = f"WAF_QT_PREFIX_{arch.upper()}"
        value = os.environ.get(env_key)
        if value:
            profiles[arch]["qt_prefix"] = _normalize_path(value)

        pack_key = f"WAF_SEVENZIP_PACK_{arch.upper()}"
        pack = os.environ.get(pack_key)
        if pack:
            profiles[arch]["sevenzip_pack_relpath"] = _normalize_path(pack)


def _apply_local_overrides(profiles):
    try:
        import waf_arch_local
    except ImportError:
        return
    apply_fn = getattr(waf_arch_local, "apply_local_overrides", None)
    if callable(apply_fn):
        apply_fn(profiles)


_apply_env_overrides(WINDOWS_PROFILES)
_apply_local_overrides(WINDOWS_PROFILES)


def binary_common_arch(target_arch):
    return BINARY_COMMON_ARCH.get(target_arch, "x86")


def get_windows_profile(target_arch):
    return WINDOWS_PROFILES.get(target_arch, WINDOWS_PROFILES["x64"])


def binary_common_subpath(root, *parts, target_arch="x64"):
    """Join path segments ending with binary_common/<x86|arm>/..."""
    arch = binary_common_arch(target_arch)
    return os.path.join(root, *parts, arch)


def adapter_product_binary_common(root, product, vendor, bus, os_name, target_arch="x64"):
    """e.g. .../flash/qc/pcie/windows/binary_common/x86"""
    return binary_common_subpath(
        root,
        "src", "afal", "adapter_product", product, vendor, bus, os_name, "binary_common",
        target_arch=target_arch,
    )


def resolve_osinfo_binary_common(root, target_arch="x64"):
    """x64 uses flat binary_common; ARM64 uses binary_common/arm when populated."""
    base = os.path.join(root, "src", "afal", "adapter_os", "osinfo", "windows", "binary_common")
    if target_arch != "arm64":
        return base
    arch_dir = os.path.join(base, binary_common_arch(target_arch))
    if os.path.isdir(arch_dir) and any(os.scandir(arch_dir)):
        return arch_dir
    return base


def detect_target_arch_from_presets(presets_data):
    for preset in presets_data.get("configurePresets", []):
        cv = preset.get("cacheVariables", {})
        if "WAF_TARGET_ARCH" in cv:
            return cv["WAF_TARGET_ARCH"]
        qt = str(cv.get("CMAKE_PREFIX_PATH", "")).lower().replace("\\", "/")
        if "/arm64" in qt or "msvc2019_arm64" in qt:
            return "arm64"
    return "x64"


def read_target_arch_from_presets_file(preset_path):
    import json

    with open(preset_path, "r", encoding="utf-8") as f:
        return detect_target_arch_from_presets(json.load(f))
