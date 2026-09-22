# /* This program is free software: you can redistribute it and/or modify
#  * it under the terms of the GNU Lesser General Public License as published by
#  * the Free Software Foundation, either version 3 of the License, or
#  * (at your option) any later version.
#  *
#  * This program is distributed in the hope that it will be useful,
#  * but WITHOUT ANY WARRANTY; without even the implied warranty of
#  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  * GNU Lesser General Public License for more details.
#  *
#  * You should have received a copy of the GNU Lesser General Public License
#  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
#  *
#  * Copyright (C) 2024, Rolling Wireless S.a.r.l.
#  */

cmake_minimum_required(VERSION 3.22)

project(qt VERSION 1.0.0 LANGUAGES CXX)


set(DEST_DIR "${CMAKE_BINARY_DIR}/release_lib/curl_lib")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/linux/x86_64")
            set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/linux/x86_64")
            file(COPY "${SOURCE_DIR}/" DESTINATION "${DEST_DIR}")
			file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../api/curl" DESTINATION "${CMAKE_BINARY_DIR}/release_lib/adapter_os/api")
            return()
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        message("copy linux aarch64 lib file")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(WAF_TARGET_ARCH STREQUAL "arm64")
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/windows/arm64")
            set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/windows/arm64")
            file(COPY "${SOURCE_DIR}/" DESTINATION "${DEST_DIR}")
            file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../api/curl" DESTINATION "${CMAKE_BINARY_DIR}/release_lib/adapter_os/api")
            return()
        endif()
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/windows/libcurl.lib")
        message("copy windows lib file")
        set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/windows")
        file(COPY "${SOURCE_DIR}/libcurl.lib" DESTINATION "${DEST_DIR}")
        if(EXISTS "${SOURCE_DIR}/libcurl.dll")
            file(COPY "${SOURCE_DIR}/libcurl.dll" DESTINATION "${DEST_DIR}")
        endif()
        file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../api/curl" DESTINATION "${CMAKE_BINARY_DIR}/release_lib/adapter_os/api")
        return()
    endif()
endif()

