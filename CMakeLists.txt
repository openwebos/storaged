# @@@LICENSE
#
#      Copyright (c) 2002-2012 Hewlett-Packard Development Company, L.P.
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


cmake_minimum_required(VERSION 2.6)

project(storaged)

set(STORAGED_VERSION_MAJOR 2)
set(STORAGED_VERSION_MINOR 1)
set(STORAGED_VERSION_PATCH 0)
set(STORAGED_PROGRAM_VERSION "${STORAGED_VERSION_MAJOR}.${STORAGED_VERSION_MINOR}.${STORAGED_VERSION_PATCH}")

# Make sure the build system's version of this component matches what we're building.
# The intent is for it to pass in STORAGED_COMPONENT_VERSION via the command line.
if(DEFINED STORAGED_COMPONENT_VERSION)
    if (NOT (STORAGED_PROGRAM_VERSION STREQUAL STORAGED_COMPONENT_VERSION))
        message(FATAL_ERROR "Component version from build system (${STORAGED_COMPONENT_VERSION}) != program version (${STORAGED_PROGRAM_VERSION})")
    endif()
endif()

# XXX Why is CMAKE_BUILD_TYPE empty under Ubuntu?
if(NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release")
endif()

if(NOT DEFINED ENV{PKG_CONFIG_PATH})
    set(ENV{PKG_CONFIG_PATH} "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig")
endif()

if(CMAKE_INSTALL_PREFIX STREQUAL "/usr")
    set(SYSCONFDIR_FULL "/etc")
else()
    set(SYSCONFDIR_FULL "${CMAKE_INSTALL_PREFIX}/etc")
endif()

message("")

message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_FIND_ROOT_PATH: ${CMAKE_FIND_ROOT_PATH}")
message(STATUS "ENV{PKG_CONFIG_PATH}: $ENV{PKG_CONFIG_PATH}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")

#  Note that TARGET_CORE_OS is no longer used, but keep it around in case it turns out to be needed later.
set(DEFAULT_TARGET_CORE_OS "ubuntu" CACHE INTERNAL "The default core OS for which to build")

# Should specify a core OS (ubuntu, rockhopper, etc.)
if(TARGET_CORE_OS)
    message(STATUS "TARGET_CORE_OS: ${TARGET_CORE_OS}")
else()
    set(TARGET_CORE_OS "${DEFAULT_TARGET_CORE_OS}")
    message(STATUS "No TARGET_CORE_OS value specified -- defaulting to ${TARGET_CORE_OS}")
endif()
set(TARGET_CORE_OS ${TARGET_CORE_OS} CACHE STRING "The core OS for which to build")

message("")


macro(add_flags flags)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flags}")
endmacro()

macro(add_linker_flags flags)
    # Appending to CMAKE_EXE_LINKER_FLAGS fails to add them to the link command line -- why?
    # Note that using add_definitions() also fails to add to the link command line.
    add_flags("${flags}")
endmacro()


include(FindPkgConfig)

# Check that we have glib-2.0
pkg_check_modules(GLIB2 REQUIRED glib-2.0)
add_definitions(${GLIB2_CFLAGS})

# Check that we have luna-service2
pkg_check_modules(LUNASERVICE2 REQUIRED luna-service2)
add_definitions(${LUNASERVICE2_CFLAGS})

# luna-service2 requires cjson; make sure it's available
pkg_check_modules(CJSON REQUIRED cjson)
add_definitions(${CJSON_CFLAGS})

# Check that we have nyx-lib
pkg_check_modules(NYXLIB REQUIRED nyx)
add_definitions(${NYXLIB_CFLAGS})

# Specifying this caused PATH_MAX not to be defined
#add_definitions("-std=c99")

# Turn on all useful warnings, but don't specify -Werror -- having warnings break the build makes
# switching to a new compiler a lengthy and painful ordeal.
add_definitions("-Wall")

# Require that all undefined symbols are satisfied by the libraries from target_link_libraries()
add_linker_flags("-Wl,--no-undefined")
# XXX_ The libraries that come with gcc fail when --no-allow-shlib-undefined is specified
#add_linker_flags("-Wl,--no-allow-shlib-undefined")


# Build the storaged executable

add_executable(storaged src/diskmode.c src/erase.c src/log.c src/main.c src/signals.c src/util.c)
target_link_libraries(storaged 
                        ${GLIB2_LDFLAGS} 
                        ${LUNASERVICE2_LDFLAGS}
                        ${CJSON_LDFLAGS}
                        ${NYXLIB_LDFLAGS})


install(TARGETS storaged DESTINATION "sbin")
install(PROGRAMS etc/udev/scripts/storage.sh DESTINATION "${SYSCONFDIR_FULL}/udev/scripts")
install(FILES etc/udev/rules.d/90-storaged.rules DESTINATION "${SYSCONFDIR_FULL}/udev/rules.d")
