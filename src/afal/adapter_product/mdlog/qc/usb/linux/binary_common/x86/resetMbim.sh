#!/bin/bash
# MBIM channel recovery via mbimcli + USB soft re-plug (unbind/bind).
# Usage: resetMbim.sh [mbim_dev_node]
# Default device: /dev/cdc-wdm0

DEV="${1:-/dev/cdc-wdm0}"
LOG="${2:-/tmp/resetMbim.log}"


GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

> "$LOG"

mbim_reset() {
	echo "  [恢复] 清理卡死的 mbimcli 进程 ..."
	killall -9 mbimcli mbim-network 2>/dev/null

	DEVICE_SYSFS=$(readlink -f /sys/class/usbmisc/$(basename "$DEV")/device)
	USB_PATH=""
	while [ -n "$DEVICE_SYSFS" ] && [ "$DEVICE_SYSFS" != "/" ]; do
		if [ -f "$DEVICE_SYSFS/busnum" ] && [ -f "$DEVICE_SYSFS/devnum" ]; then
			USB_PATH=$(basename "$DEVICE_SYSFS")
			break
		fi
		DEVICE_SYSFS=$(dirname "$DEVICE_SYSFS")
	done

	if [ -z "$USB_PATH" ]; then
		echo "  [恢复] 失败: 无法解析 $DEV 对应的 USB 路径"
		return 1
	fi
	echo "  [恢复] USB 路径: $USB_PATH,执行 unbind/bind ..."

	echo "$USB_PATH" > /sys/bus/usb/drivers/usb/unbind
	sleep 2
	echo "$USB_PATH" > /sys/bus/usb/drivers/usb/bind
	sleep 2

	# 等待设备节点重新出现，最多 15 秒
	for t in $(seq 1 15); do
		[ -e "$DEV" ] && break
		sleep 1
	done
	if [ ! -e "$DEV" ]; then
		echo "  [恢复] 失败: 等待 $DEV 重新出现超时"
		return 1
	fi

	echo "  [恢复] $DEV 已重新枚举，等待 MBIM 初始化 ..."
	sleep 5
	return 0
}

if [ "$(id -u)" != "0" ]; then
	echo "Please run as root"
	exit 1
fi

# 最多 4 次 AT 探测；失败时最多做 3 次 USB reset，第 3 次 reset 后再探测一次
MAX_RESET=3
RETRY_OK=0
for attempt in $(seq 0 "$MAX_RESET"); do
	output=$(mbimcli -p -d "$DEV" --fibocom-set-at-command='AT' 2>&1)
	if echo "$output" | grep -q "OK"; then
		RETRY_OK=1
		break
	fi

	echo "  mbimcli 第$((attempt + 1))次失败，输出: $output" >> "$LOG"
	echo "  mbimcli 第$((attempt + 1))次失败"

	if [ "$attempt" -eq "$MAX_RESET" ]; then
		break
	fi

	echo "  执行第$((attempt + 1))次 USB 复位恢复 ..."
	if ! mbim_reset; then
		echo "SWITCH_TO_SHOW_AT_PORT,FAIL,ERROR,N/A,N/A,1"
		exit 1
	fi
	# 递增等待：第1次等2秒，第2次等4秒，第3次等6秒
	sleep $(((attempt + 1) * 2))
done

if [ "$RETRY_OK" -eq 1 ]; then
	echo -e "${GREEN}SWITCH_TO_SHOW_AT_PORT,PASS,OK,N/A,N/A,0${NC}"
	exit 0
fi

# 三次 USB reset 后仍失败
echo -e "${RED}SWITCH_TO_SHOW_AT_PORT,FAIL,ERROR,N/A,N/A,1${NC}"
echo "OUTPUT: $output" >&2
exit 1
