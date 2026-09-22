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
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_SOURCE_DIR}/cmake/WafArch.cmake)

# 设置安装路径为当前构建目录下的 install 子目录

# Add directory compilation
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	add_subdirectory(common)
	add_subdirectory(diag)
	add_subdirectory(dump)
	add_subdirectory(flash)
    add_subdirectory(lpa)
	add_subdirectory(mdlog)
	#add_subdirectory(tx_agent)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	if(NOT WAF_TARGET_ARCH STREQUAL "arm64")
		add_subdirectory(miniDebugLogger)
	endif()
	add_subdirectory(common)
	#add_subdirectory(diag)
	add_subdirectory(dump)
	add_subdirectory(flash)
	add_subdirectory(lpa)
	add_subdirectory(mdlog)
	add_subdirectory(snappy_lib)
	# add_subdirectory(tx_agent)
endif()



