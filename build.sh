#!/bin/bash

OEM_LIST=(hp dell lenovo generic chrome)
RELEASE_TYPE=(service tool lib cli all)
INSTALL_TYPE=(deb rpm run)

find_oem="no"
find_install="no"

function find_linux_service_preset_for_tool() {
    local tool_preset="$1"
    local pkg_type="${tool_preset##*-}"

    if [[ "$tool_preset" != tool-* && "$tool_preset" != tools-* ]]; then
        return
    fi
    if [ ! -f "./CMakePresets.json" ]; then
        return
    fi

    python3 - "$pkg_type" <<'PY'
import json
import sys

pkg = sys.argv[1]
with open("CMakePresets.json") as f:
    names = [p["name"] for p in json.load(f)["configurePresets"]]

services = [n for n in names if "-service-" in n and n.endswith("-" + pkg)]
generic = [n for n in services if "generic" in n]
if generic:
    print(generic[0])
elif len(services) == 1:
    print(services[0])
PY
}

function resolve_linux_tool_build_presets() {
    local tool_preset="$1"
    local service_preset

    service_preset=$(find_linux_service_preset_for_tool "$tool_preset")
    if [ -n "$service_preset" ]; then
        echo "$service_preset"
    fi
    echo "$tool_preset"
}

function build_cpack_preset() {
    local preset="$1"

    echo "Executing cmake with preset: ${preset}"
    if [ -d "build" ]; then
        rm -rf build
    fi
    cmake --preset "${preset}"
    if [ $? -ne 0 ]; then
        return 1
    fi
    cmake --build build -- -v 2>&1 | tee build/build_output.log
    EXIT_CODE=${PIPESTATUS[0]}
    if [ $EXIT_CODE -ne 0 ]; then
        return 1
    fi
    cpack --config build/CPackConfig.cmake
    if [ $? -ne 0 ]; then
        return 1
    fi

    if [[ "${preset}" == *"deb"* ]]; then
        cp build/*.deb build_release
    elif [[ "${preset}" == *"rpm"* ]]; then
        cp build/*.rpm build_release
    elif [[ "${preset}" == *"run"* ]]; then
        for file_path in ./build/*.tar.gz; do
            file_name=$(basename "$file_path" .tar.gz)
            echo "The tar.gz file name is: $file_name"
        done
        cp ./script/linux/cpack_source/run/create_install.sh ./build
        cat ./build/create_install.sh ./build/${file_name}.tar.gz > ./build/${file_name}.run
        chmod +x ./build/${file_name}.run
        cp build/*.run build_release
        echo "create run file success!"
    fi
    return 0
}

function check_and_config() {
    input=$1
    OLD_IFS="$IFS"
    IFS='-'
    read -ra parts <<< "$input"
    IFS="$OLD_IFS"
    if [[ ${parts[0]} =~ "Test" ]] || [[ ${parts[0]} =~ "tool" ]] || [[ ${parts[0]} =~ "cli" ]]; then
        echo "${parts[0]} pass"
    else
        for list1 in "${INSTALL_TYPE[@]}"; do
            if [ "$list1" == "${parts[3]}" ]; then
                find_install="yes"
                break
            fi
        done
        if [ "$find_install" != "yes" ]; then
            return 0
        fi
        for list in "${OEM_LIST[@]}"; do
            if [ "$list" == "${parts[0]}" ]; then
                if [[ ${parts[1]} =~ "rw" ]] || [[ ${parts[1]} =~ "fm" ]]; then
                    find_oem="yes"
                    break
                fi
            fi
        done
        if [ "$find_oem" == "yes" ]; then
            if [ "${parts[2]}" == "service" ] || [ "${parts[2]}" == "all" ]; then
                for list1 in "${INSTALL_TYPE[@]}"; do
                    if [ "$list1" == "${parts[3]}" ]; then
                        find_install="yes"
                        break
                    fi
                done
                if [ "$find_install" != "yes" ]; then
                    return 0
                fi
            else
                return 0
            fi
        else
            return 0
        fi
    fi

    echo "All parameters are correct, now configuring..."

    if [ -f "./CMakePresets.json" ]; then
        rm -rf ./CMakePresets.json
    fi
    if [ -d "build" ]; then
        rm -rf build
    fi

    if [[ ${parts[1]} =~ "rw" ]] || [[ ${parts[1]} =~ "fm" ]]; then
        cp "./build_default_config/linux/${parts[1]}/CMakePresets.json" ./
    else
        cp "./build_default_config/linux/CMakePresets.json" ./
    fi

    if [[ "$1" == *"all"* ]]; then
        if [ -f "CMakePresets.json" ]; then
            res=`grep -oP '"configurePreset":\s*"\K[^"]+' CMakePresets.json`

            presets=(${res})
            if [ -d "build_release" ]; then
                rm -rf build_release
            fi
            mkdir -p build_release

            if [[ "$1" == *"deb"* ]]; then
                field="deb"
            elif [[ "$1" == *"rpm"* ]]; then
                field="rpm"
            elif [[ "$1" == *"run"* ]]; then
                field="deb"
            fi

            oem=''
            for list in "${OEM_LIST[@]}"; do
                if [ "$list" == "${parts[0]}" ]; then
                    oem=${parts[0]}
                fi
            done

            for preset in "${presets[@]}"; do
                if [[ "${preset}" == *$field* ]]; then
                    if [[ "${preset}" == *$oem* ]] || [[ "${preset}" == *"tool"* ]]\
                        || [[ "${preset}" == *"cli"* ]]; then
                        echo "Executing cmake with preset: ${preset}"
                        if [ -d "build" ]; then
                            rm -rf build
                        fi
                        cmake --preset "${preset}"
                        if [ $? -ne 0 ]; then
                            return 1
                        fi
                        # cmake --build build -- -v 2>&1 | tee build/build_output.log
                        cmake --build build -- -v 2>&1 | tee build/build_output.log
                        EXIT_CODE=${PIPESTATUS[0]}
                        if [ $EXIT_CODE -ne 0 ]; then
                            exit 1
                        fi
                        cpack --config build/CPackConfig.cmake
                        if [ $? -ne 0 ]; then
                            return 1
                        fi
                        if [[ "${preset}" == *"deb"* ]]; then
                            cp build/*.deb build_release
                        elif [[ "${preset}" == *"rpm"* ]]; then
                            cp build/*.rpm build_release
                        elif [[ "${res}" == *"run"* ]]; then
                            for file_path in ./build/*.tar.gz; do
                                file_name=$(basename "$file_path" .tar.gz)
                                echo "The tar.gz file name is: $file_name"
                            done
                            cp ./script/linux/cpack_source/run/create_install.sh ./build
                            cat ./build/create_install.sh ./build/${file_name}.tar.gz > ./build/${file_name}.run
                            chmod +x ./build/${file_name}.run
                            cp build/*.run build_release
                            echo "create run file success!"
                        fi
                    fi
                    
                fi
            done
        else
            echo "current CMakePresets not exit."
        fi
    elif [[ "$1" =~ "Test" ]]; then
        cmake --preset "$1"
        if [ $? -ne 0 ]; then
            return 1
        fi
        cmake --build build -- -v 2>&1 | tee build/build_output.log
        EXIT_CODE=${PIPESTATUS[0]}
        if [ $EXIT_CODE -ne 0 ]; then
            exit 1
        fi
        ctest --test-dir build
    else
        if [ -d "build_release" ]; then
            rm -rf build_release
        fi
        mkdir -p build_release

        presets_to_build=("$1")
        if [[ "$1" == tool-* || "$1" == tools-* ]]; then
            mapfile -t presets_to_build < <(resolve_linux_tool_build_presets "$1")
        fi

        for preset in "${presets_to_build[@]}"; do
            build_cpack_preset "${preset}"
            if [ $? -ne 0 ]; then
                return 1
            fi
        done
    fi
    return 1
}

if [ $# -eq 1 ]; then
    if [ "$1" == "config" ]; then
        if [ -f "CMakePresets.json" ]; then
            rm -rf CMakePresets.json
        fi

        python3 ./script/config_project.py
        if [ -f "CMakePresets.json" ]; then
            res=`grep -oP '"configurePreset":\s*"\K[^"]+' CMakePresets.json`

            presets=(${res})
            if [ -d "build_release" ]; then
                rm -rf build_release
            fi
            mkdir -p build_release

            for preset in "${presets[@]}"; do
                echo "Executing cmake with preset: ${preset}"
                if [ -d "build" ]; then
                    rm -rf build
                fi
                cmake --preset "${preset}"
                if [ $? -ne 0 ]; then
                    exit 1
                fi
                cmake --build build -- -v 2>&1 | tee build/build_output.log
                EXIT_CODE=${PIPESTATUS[0]}
                if [ $EXIT_CODE -ne 0 ]; then
                    exit 1
                fi

                if [[ "$preset" =~ "Test" ]]; then
                    ctest --test-dir build
                else
                    cpack --config build/CPackConfig.cmake
                    if [ $? -ne 0 ]; then
                        exit 1
                    fi
                    if [[ "${preset}" == *"deb"* ]]; then
                        cp build/*.deb build_release
                    elif [[ "${preset}" == *"rpm"* ]]; then
                        cp build/*.rpm build_release
                    elif [[ "${res}" == *"run"* ]]; then
                        for file_path in ./build/*.tar.gz; do
                            file_name=$(basename "$file_path" .tar.gz)
                            echo "The tar.gz file name is: $file_name"
                        done
                        cp ./script/linux/cpack_source/run/create_install.sh ./build
                        cat ./build/create_install.sh ./build/${file_name}.tar.gz > ./build/${file_name}.run
                        chmod +x ./build/${file_name}.run
                        cp build/*.run build_release
                        echo "create run file success!"
                    fi
                fi
            done
        else
            echo "current CMakePresets not exit."
        fi
    else 
        check_and_config "$1"
        if [ $? -eq 0 ]; then
            echo "Input parameter error, example: ./build.sh lenovo-rw135-service-rpm or ./build.sh config"
        fi
    fi
else
    echo "Usage: ./build.sh <project-config> or ./build.sh config"
fi
