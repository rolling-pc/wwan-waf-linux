#!/bin/bash

path="/etc/init"
#stop service
if [[ -f ${path}/wwan_dev.conf && -f ${path}/wwan_func.conf ]];then
    stop wwan_tool
    stop wwan_func
    stop wwan_dev
fi


# Get the current script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Get the parent directory of the script directory
PARENT_DIR="$(dirname "${SCRIPT_DIR}")"

TARGET_DIR="/etc/init"
if [ -d "$TARGET_DIR" ]; then
    echo "Deleting the waf service file: ${TARGET_DIR}/wwan_dev.conf ${TARGET_DIR}/wwan_dev.conf"
    rm -rf "${TARGET_DIR}/wwan_dev.conf"
    rm -rf "${TARGET_DIR}/wwan_func.conf"
    rm -rf "${TARGET_DIR}/wwan_tool.conf"
else
    echo "Directory $SOURCE_DIR does not exist. No files remove."
fi

# Check if the current directory is 'scripts' and the parent is 'waf'
if [ "$(basename "${SCRIPT_DIR}")" == "script" ] && [ "$(basename "${PARENT_DIR}")" == "waf" ]; then
    echo "Deleting the waf directory: ${PARENT_DIR}"
    rm -rf "${PARENT_DIR}"
else
    echo "Current directory is not 'script' or parent is not 'waf'. No action taken."
fi


echo "uninstall wwan service success!"