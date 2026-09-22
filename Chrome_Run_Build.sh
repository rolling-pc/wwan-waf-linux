#!/bin/bash
# generate_run.sh
# 构建跨架构自包含 installer

set -e

if [ $# -lt 1 ]; then
    echo "Usage: $0 <docker_run_dir>"
    exit 1
fi

DOCKER_DIR="$1"
BUILD_DIR="./build_release"
WRAPPER_TEMP="./Run_Temp"

# 1. 清理旧目录
rm -rf "$WRAPPER_TEMP"
mkdir -p "$WRAPPER_TEMP"

# 2. 构建 x86_64 版本
echo "[*] Building x86_64 binary..."
./enter_ubuntu_2204_Auto.sh "$DOCKER_DIR"

X86_RUN=$(ls "$BUILD_DIR"/wwan-service-tool-generic-*-x86_64.run 2>/dev/null | head -n1)
if [ -z "$X86_RUN" ]; then
    echo "[!] x86_64 run file not found"
    exit 1
fi
echo "[*] Found x86_64 binary: $X86_RUN"
cp "$X86_RUN" "$WRAPPER_TEMP/"

# 3. 提取版本号（此时build_release还存在）
X86_BASENAME=$(basename "$X86_RUN")
VERSION=${X86_BASENAME#wwan-service-tool-generic-}
VERSION=${VERSION%-x86_64.run}
echo "$VERSION" > "$WRAPPER_TEMP/version.txt"
echo "[*] Detected version: $VERSION"

# 4. 构建 aarch64 版本
echo "[*] Building aarch64 binary..."
./enter_ARM64_2204_Auto.sh "$DOCKER_DIR"

ARM_RUN=$(ls "$BUILD_DIR"/wwan-service-tool-generic-*-aarch64.run 2>/dev/null | head -n1)
if [ -z "$ARM_RUN" ]; then
    echo "[!] aarch64 run file not found"
    exit 1
fi
echo "[*] Found aarch64 binary: $ARM_RUN"
cp "$ARM_RUN" "$WRAPPER_TEMP/"


# 5. 生成最终 wrapper
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
FINAL_RUN="$BUILD_DIR/wwan-service-tool-generic-$VERSION.run"
echo "[*] Generating wrapper: $FINAL_RUN"

cat > "$FINAL_RUN" << 'EOF'
#!/bin/bash
set -e

# 临时解压目录
TMP_BASE="/usr/local/temp"
TMP_DIR="$TMP_BASE/wwan_tmp_$$"

# 检查写权限
if [ ! -d "$TMP_BASE" ]; then
    sudo mkdir -p "$TMP_BASE"
fi

if [ ! -w "$TMP_BASE" ]; then
    echo "[!] No write permission to $TMP_BASE, please run with sudo"
    exit 1
fi

mkdir -p "$TMP_DIR"

# 架构检测
ARCH=$(uname -m)

echo "[*] Extracting installer to $TMP_DIR ..."
TAIL_LINE=$(grep -an "^__ARCHIVE_BELOW__$" "$0" | cut -d: -f1)
tail -n +$((TAIL_LINE+1)) "$0" | tar xz -C "$TMP_DIR"

chmod +x "$TMP_DIR"/wwan-service-tool-generic-*.run 2>/dev/null || true

VERSION="unknown"
if [ -f "$TMP_DIR/version.txt" ]; then
    VERSION=$(cat "$TMP_DIR/version.txt")
fi

if [ "$EUID" -ne 0 ]; then
    echo "[!] Root privileges required to install to /usr/local"
    echo "[!] Please re-run with: sudo $0"
    exit 1
fi

case "$ARCH" in
    x86_64|i386)
        INNER="$TMP_DIR/wwan-service-tool-generic-$VERSION-x86_64.run"
        ;;
    aarch64)
        INNER="$TMP_DIR/wwan-service-tool-generic-$VERSION-aarch64.run"
        ;;
    *)
        echo "[!] Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

echo "[*] Installing WWAN Service Tool version $VERSION for $ARCH ..."
bash "$INNER" || {
    echo "[!] Internal installer failed"
    exit 1
}

# 清理临时目录
rm -rf "$TMP_DIR"
exit 0

__ARCHIVE_BELOW__
EOF


# 6. 打包所有文件
tar czf - -C "$WRAPPER_TEMP" . >> "$FINAL_RUN"

# 7. 添加执行权限
chmod +x "$FINAL_RUN"

# 8. 清理临时目录
#rm -rf "$WRAPPER_TEMP"

echo "[*] Done!"
echo "[*] Final installer: $FINAL_RUN"