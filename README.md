# Rolling WAF (Linux)
This is a Rolling WAF (WWAN Application Framework) project for WWAN devices on Linux.<br>
  **Device service (`wwan_dev_service`):** device and port detection, device information, hardware reset.<br>
  **Function service (`wwan_func_service`):** firmware update, switch, recovery, eSIM, configuration.<br>
  **AFAL:** adaptation layer providing MBIM, udev device, log, power and display.<br>

The two services are built from the open sources plus the prebuilt private libraries under
`prebuilt/linux/x86_64/` (their source is not distributed). `cmake/WafPrivateDeps.cmake` imports
them during configuration.<br>

# License
See `COPYING`.<br>
The prebuilt libraries under `prebuilt/` are proprietary binaries; their source code is not distributed.<br>

# Notice
  - Firmware update requires an fw_package placed under `/etc/opt/waf/waf_fw_pkg/`. Obtain the fw package from the corresponding OEM.<br>
  - Flashing uses `libflash_qc.so` and `tools/qc/qdl` from the package, plus the commands `mbimcli` and `lspci`; hardware reset uses `dmidecode`.<br>
  - Qt5, ICU, curl and OpenSSL runtime libraries are bundled in the deb.<br>
  - Runs on Ubuntu 22.04 (amd64) and newer; other OSes are unverified.<br>

# Building on Ubuntu

## 1. Install

- sudo apt update<br>
- sudo apt install -y cmake ninja-build build-essential pkg-config<br>
- sudo apt install -y qtbase5-dev libglib2.0-dev libudev-dev libmbim-glib-dev<br>

## 2. Build

1. `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCURRENT_PROJECT=Qualcomm -DCURRENT_OEM=generic -DCURRENT_PACKAGE_TYPE=deb -DINSTALL_SERVICE_ENABLE=ON -DINSTALL_TOOLS_ENABLE=OFF -DINSTALL_CLI_ENABLE=OFF -DPROJECT_PLUGIN_MA=OFF`<br>
2. `cmake --build build -j"$(nproc)"`<br>
3. Or one step: `./build.sh config`<br>

## 3. Make the deb package

1. `cd build`<br>
2. `cpack --config CPackConfig.cmake`<br>
3. Generated package: `wwan-service-generic_1.0.21_amd64.deb`<br>

The package name is composed in `script/linux/cpack_cfg/service_CPackConfig.cmake` from
`CPACK_PACKAGE_NAME` (`wwan-service-${CURRENT_OEM}`), `CPACK_PACKAGE_VERSION` and the deb architecture.<br>

## 4. Deploy on the target machine

1. `sudo apt install -y libmbim-glib4 libmbim-utils pciutils dmidecode`<br>
2. `sudo dpkg -i wwan-service-generic_1.0.21_amd64.deb`<br>
3. Check the services: `systemctl is-active wwan_dev.service wwan_func.service`<br>
4. Logs: `journalctl -u wwan_dev.service -n 50 --no-pager`<br>
5. Uninstall: `sudo dpkg -r wwan-service-generic`<br>