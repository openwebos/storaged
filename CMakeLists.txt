# @@@LICENSE
#
#      Copyright (c) 2002-2013 LG Electronics, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# LICENSE@@@

#
# storaged/CMakeLists.txt
#


cmake_minimum_required(VERSION 2.8.7)

project(storaged C)

include(webOS/webOS)
webos_modules_init(1 0 0 QUALIFIER RC4)
webos_component(2 2 0)

include(FindPkgConfig)

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${GLIB2_CFLAGS_OTHER})

pkg_check_modules(LUNASERVICE2 REQUIRED luna-service2)
include_directories(${LUNASERVICE2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${LUNASERVICE2_CFLAGS_OTHER})

pkg_check_modules(CJSON REQUIRED cjson)
include_directories(${CJSON_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${CJSON_CFLAGS_OTHER})

pkg_check_modules(NYXLIB REQUIRED nyx)
include_directories(${NYXLIB_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${NYXLIB_CFLAGS_OTHER})

# Nyxlib version 4.0.0~rc1 introduced an API break in Mass Storage Mode usage
string(REPLACE "." ";" NYXV ${NYXLIB_VERSION})
string(REPLACE "~" ";" NYXV "${NYXV}")
list(GET NYXV 0 NYXLIB_MAJOR)
if(${NYXLIB_MAJOR} LESS 4)
	message(FATAL_ERROR "nyx-lib required minimum version is 4.0.0~RC1")
endif()
unset(NYXV)
unset(NYXLIB_MAJOR)

webos_add_compiler_flags(ALL -Wall)

# Require that all undefined symbols are satisfied by the libraries from target_link_libraries()
webos_add_linker_options(ALL --no-undefined)

# Locations of mass storage mode pre/post-change scripts
add_definitions(-DPREMSM_SCRIPT_DIR="${WEBOS_INSTALL_SYSCONFDIR}/storaged/pre_msm.d")
add_definitions(-DPOSTMSM_SCRIPT_DIR="${WEBOS_INSTALL_SYSCONFDIR}/storaged/post_msm.d")

# Build the storaged executable

add_executable(storaged src/diskmode.c src/erase.c src/log.c src/main.c src/signals.c src/util.c)
target_link_libraries(storaged 
                        ${GLIB2_LDFLAGS} 
                        ${LUNASERVICE2_LDFLAGS}
                        ${CJSON_LDFLAGS}
                        ${NYXLIB_LDFLAGS})
webos_build_program(ADMIN)
webos_build_system_bus_files()

webos_configure_source_files(configuredfile scripts/public/storage.sh)
install(PROGRAMS ${configuredfile}
    DESTINATION ${WEBOS_INSTALL_SYSCONFDIR}/udev/scripts)

webos_build_configured_file(files/conf/90-storaged.rules SYSCONFDIR udev/rules.d)
