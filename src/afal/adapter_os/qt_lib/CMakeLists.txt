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


set(DEST_DIR "${CMAKE_BINARY_DIR}/release_lib/qt_lib")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/linux/x86_64")
            set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/linux/x86_64")
            file(COPY "${SOURCE_DIR}/" DESTINATION "${DEST_DIR}")
            return()
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/linux/aarch64")
            set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/linux/aarch64")
            file(COPY "${SOURCE_DIR}/" DESTINATION "${DEST_DIR}")
            return()
        endif()
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/windows")
        message("copy windows lib file")
        return()
    endif()
endif()

