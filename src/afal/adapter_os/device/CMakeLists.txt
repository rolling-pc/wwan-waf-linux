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
#根据传入的编译宏决定使用linux还windws
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/linux")
      add_subdirectory(linux)
    endif()


elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/windows")
		add_subdirectory(windows)
    endif()
endif()