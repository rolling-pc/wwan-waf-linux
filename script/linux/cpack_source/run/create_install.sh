#!/bin/bash

# Default directory
TARGET_DIR="${1:-/usr/local/opt}"

usage() {
  echo "Usage: $0 [options]"
  echo "Options:"
  echo "  --target-dir=DIR  Set the target directory for installation (default: /usr/local/opt)"
  echo "  --help            Show this help message"
  exit 1
}

for arg in "$@"; do
  case $arg in
    --target-dir=*)
      TARGET_DIR="${arg#*=}"
      shift
      ;;
    --help)
      usage
      ;;
    *)
      echo "Unknown option: $arg"
      usage
      ;;
  esac
done

PACKET_PATH=${TARGET_DIR}/waf/script
if [ -d "${PACKET_PATH}" ]; then
    echo "Directory ${PACKET_PATH} exists."
    
    # Check if the uninstall script exists and is executable
    if [ -x "${PACKET_PATH}/uninstall.sh" ]; then
        echo "Running uninstall script..."
        "${PACKET_PATH}/uninstall.sh"
    else
        echo "Uninstall script not found or not executable: ${PACKET_PATH}/uninstall.sh"
    fi
else
    echo "Directory ${PACKET_PATH} does not exist. No action taken."
fi

echo "Extracting files to ${TARGET_DIR}..."
mkdir -p "${TARGET_DIR}"

# find __END__ flag
END_POS=$(grep -an '^__END__$' "$0" | cut -d: -f1)
if [ -z "$END_POS" ]; then
  echo "__END__ marker not found in the script."
  exit 1
fi

# after __END__ 
tail -n +$((END_POS + 1)) "$0" | tar xz --strip-components=1 -C "${TARGET_DIR}"

echo "install successfully,install path ${TARGET_DIR}"
# Run installation script
echo "Running installation script..."
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
SOURCE_DIR=${TARGET_DIR}/waf/etc/init
CONF_TARGET_DIR="/etc/init"

if [ -d "$SOURCE_DIR" ]; then
    echo "Directory $SOURCE_DIR exists. Copying files to $CONF_TARGET_DIR..."

    if [ ! -d "$CONF_TARGET_DIR" ]; then
        echo "Creating target directory $CONF_TARGET_DIR..."
        mkdir -p "$CONF_TARGET_DIR"
    fi

    mv "$SOURCE_DIR/"* "$CONF_TARGET_DIR/"
    rm -rf ${TARGET_DIR}/waf/etc/

else
    echo "Directory $SOURCE_DIR does not exist. No files copied."
fi

path=${CONF_TARGET_DIR}
if [[ -f "${path}/wwan_dev.conf" && -f "${path}/wwan_func.conf" ]]; then
    if status wwan_dev | grep -q "running" && \
       status wwan_func | grep -q "running" && \
       status wwan_tool | grep -q "running"; then
        echo "wwan_dev, wwan_func, and wwan_tool are already running!"
    else
        # 尝试启动 wwan_dev、wwan_func 和 wwan_tool
        if start wwan_dev && start wwan_func && start wwan_tool; then
            echo "wwan_dev, wwan_func, and wwan_tool started successfully!"
        else
            echo "Failed to start wwan_dev, wwan_func, or wwan_tool!"
        fi
    fi
else
    echo "Required configuration files are missing in path: ${path}"
fi

echo "install wwan service success!"
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
exit 0

# Self-extracting archive starts here
__END__
