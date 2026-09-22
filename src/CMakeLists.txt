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

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	if (INSTALL_TOOLS_ENABLE AND NOT INSTALL_SERVICE_ENABLE
		AND (CURRENT_PACKAGE_TYPE STREQUAL "deb" OR CURRENT_PACKAGE_TYPE STREQUAL "rpm"))
		add_subdirectory(./wtf)
		add_subdirectory(./wsf/wwan_dev_service/wwan_dev_lib)
		add_subdirectory(./wsf/wwan_func_service/wwan_func_lib)
		add_subdirectory(./wsf/wwan_func_service/wwan_func_lib_tool)
	endif()
else()
	if (INSTALL_TOOLS_ENABLE AND CURRENT_PACKAGE_TYPE STREQUAL "exe")
		add_subdirectory(./wtf)
		#add_subdirectory(./wsf/wwan_dev_service/wwan_dev_lib)
		#add_subdirectory(./wsf/wwan_func_service/wwan_func_lib)
		#add_subdirectory(./wsf/wwan_func_service/wwan_func_lib_tool)
	endif()
endif()

if (INSTALL_DRIVER_ENABLE)
    add_subdirectory(./driver)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	add_subdirectory(afal)
	if(CURRENT_PACKAGE_TYPE STREQUAL "run")
		add_subdirectory(wtf)
		add_subdirectory(wsf)
	else()
		if (INSTALL_SERVICE_ENABLE)
			add_subdirectory(wsf)
		endif()
	endif()
	
else()
    if (NOT INSTALL_SERVICE_ENABLE AND INSTALL_CLI_ENABLE)
		add_subdirectory(afal)
		add_subdirectory(wtf/cli)
	else() 
		add_subdirectory(afal)
		if (INSTALL_SERVICE_ENABLE)
			add_subdirectory(wsf)
		endif()
	endif()
endif()

# "CMAKE_C_COMPILER": "/usr/bin/aarch64-linux-gnu-gcc",
# "CMAKE_CXX_COMPILER": "/usr/bin/aarch64-linux-gnu-g++",