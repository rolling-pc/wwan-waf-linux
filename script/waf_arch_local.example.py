"""Machine-local Windows build paths (optional).

Copy this file to ``script/waf_arch_local.py`` (gitignored) and edit paths
for your PC. Repo defaults in ``waf_arch.py`` stay suitable for Linux/CI.

Alternatively set environment variables (override local file):
  WAF_QT_PREFIX_X64=d:/Qt/5.15.2/msvc2019_64
  WAF_QT_PREFIX_ARM64=d:/Qt/5.15.2/msvc2019_arm64

7-Zip defaults are repo-relative under ``third_party/windows/7zip/`` and
usually need no local override:
  x86/ — host tool to build WwanToolKit.zip on the x64 build machine
  arm/ — binaries staged into the ARM64 MSI for install.bat on device

Optional 7-Zip overrides (only if your copies live outside the repo):
  WAF_SEVENZIP_HOST=d:/path/to/7zip-x86
  WAF_SEVENZIP_PACK_X64=third_party/windows/7zip/x86
  WAF_SEVENZIP_PACK_ARM64=third_party/windows/7zip/arm
"""


def apply_local_overrides(profiles):
    profiles["x64"]["qt_prefix"] = "d:/Qt/5.15.2/msvc2019_64"
    profiles["arm64"]["qt_prefix"] = "d:/Qt/5.15.2/msvc2019_arm64"

    # 7-Zip: uncomment only when paths differ from waf_arch.py defaults.
    # profiles["x64"]["sevenzip_host_relpath"] = "third_party/windows/7zip/x86"
    # profiles["x64"]["sevenzip_pack_relpath"] = "third_party/windows/7zip/x86"
    # profiles["arm64"]["sevenzip_host_relpath"] = "third_party/windows/7zip/x86"
    # profiles["arm64"]["sevenzip_pack_relpath"] = "third_party/windows/7zip/arm"
