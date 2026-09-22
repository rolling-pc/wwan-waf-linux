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

include(InstallRequiredSystemLibraries)

string(FIND "${CURRENT_PROJECT}" "," COMMA_POSITION)

set(PROJECT,"")
if(COMMA_POSITION GREATER -1)
    set(PROJECT "multiple")
else()
    set(PROJECT "${CURRENT_PROJECT}")
endif()
message(STATUS "Single project: ${PROJECT}")

if(INSTALL_SERVICE_ENABLE)
    set(CPACK_PACKAGE_NAME "wwan-service-${PROJECT}-${CURRENT_OEM}")
    set(CPACK_PACKAGE_VERSION "1.0.0")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "wwan_service-${CURRENT_PROJECT}-${CURRENT_OEM}")
elseif(INSTALL_TOOLS_ENABLE)
    set(CPACK_PACKAGE_NAME "wwan-tool")
    set(CPACK_PACKAGE_VERSION "1.0.0")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "wwan-tool")
elseif(INSTALL_CLI_ENABLE)
    set(CPACK_PACKAGE_NAME "wwan-cli")
    set(CPACK_PACKAGE_VERSION "1.0.0")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "wwan-cli")
endif()

set(CPACK_PACKAGE_VENDOR "xxx")
set(CPACK_PACKAGE_CONTACT "xxx@rolling.com")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}")



# set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
# set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")


# DEB Package Configuration
if("${CURRENT_PACKAGE_TYPE}" STREQUAL "deb")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.4)")
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "x86_64")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION  "WWAN Linux wwan_cli")
    set(CPACK_DEBIAN_PACKAGE_SECTION "net")
    set(CPACK_DEBIAN_PACKAGE_SECTION "base")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "standard")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "rolling")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "test <test@rolling.com>")
elseif("${CURRENT_PACKAGE_TYPE}" STREQUAL "rpm")
    # set(CPACK_RPM_PACKAGE_REQUIRES "libc6 >= 2.4")
    set(CPACK_GENERATOR "RPM")
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
    set(CPACK_RPM_PACKAGE_SUMMARY "WWAN Linux wwan_cli")
    set(CPACK_RPM_PACKAGE_DEBUG FALSE)
    set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Internet")
    set(CPACK_RPM_PACKAGE_DESCRIPTION "WWAN Linux wwan_cli")
    set(CPACK_RPM_PACKAGE_LICENSE "LGPL3.0")
    set(CPACK_RPM_RELOCATION_PATHS "/lib/systemd/system;/opt/waf")
    set(CPACK_RPM_PACKAGE_MAINTAINER "test <test@rolling.com>")
elseif("${CURRENT_PACKAGE_TYPE}" STREQUAL "run")
    # 使用 STGZ 生成器创建 .run 安装包
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-x86_64")
else()
    message("unknow package type")
endif()


include(CPack)
