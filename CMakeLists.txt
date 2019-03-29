# Copyright 2018 WolkAbout Technology s.r.o.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.5)
project(WolkGatewayBluetoothModule C CXX)

set_property(GLOBAL PROPERTY ALLOW_DUPLICATE_CUSTOM_TARGETS 1)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(NO_OUTPUT_DIRS KEEP_RPATHS)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS}    -Wall -Wextra -pedantic                                                    \
                                            -pedantic-errors -Wcast-align                                              \
                                            -Wcast-qual -Wconversion                                                   \
                                            -Wdisabled-optimization                                                    \
                                            -Wfloat-equal -Wformat=2                                                   \
                                            -Werror=init-self                                                          \
                                            -Werror=missing-field-initializers -Wmissing-format-attribute              \
                                            -Wmissing-include-dirs -Wmissing-noreturn                                  \
                                            -Werror=pointer-arith                                                      \
                                            -Wno-packed  -Wno-padded -Wredundant-decls                                 \
                                            -Werror=shadow -Werror=stack-protector                                     \
                                            -Wstrict-aliasing=2                                                        \
                                            -Werror=unreachable-code -Wno-unused                                       \
                                            -Wvariadic-macros                                                          \
                                            -Wwrite-strings -Werror=non-virtual-dtor")

set(SSL_INCLUDE_DIR "${CONAN_INCLUDE_DIRS_OPENSSL}")
set(SSL_LIBS "${CONAN_LIBS}")

# library output
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
link_directories(${CMAKE_BINARY_DIR}/lib)

# WolkAbout C++ Module
add_subdirectory(WolkGatewayModule-Cpp lib)

# WolkAbout Bluetooth Module
include_directories("src")

file(GLOB_RECURSE HEADER_FILES "src/*.h" "src/*.hpp")
file(GLOB_RECURSE SOURCE_FILES "src/*.cpp" "example/*.cpp")

#glib
FIND_PACKAGE(PkgConfig)
PKG_CHECK_MODULES(GLIB glib-2.0)
include_directories(${GLIB_INCLUDE_DIRS})
link_directories(${GLIB_LIBRARY_DIRS})
add_definitions(${GLIB_CFLAGS_OTHER})

IF(GLIB_FOUND)
    message("Glib found!")
    message("${GLIB_LIBRARIES}")
ENDIF()

#gio
PKG_CHECK_MODULES(GIO gio-2.0)
include_directories(${GIO_INCLUDE_DIRS})
link_directories(${GIO_LIBRARY_DIRS})
add_definitions(${GIO_CFLAGS_OTHER})

IF(GIO_FOUND)
    message("Gio found!")
    message("${GIO_LIBRARIES}")
ENDIF()


add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} WolkGatewayModule ${GLIB_LIBRARIES} ${GIO_LIBRARIES})
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-Wl,-rpath,./")

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")

# Example
include_directories("application")

file(GLOB_RECURSE EXAMPLE_HEADER_FILES "application/*.h")
file(GLOB_RECURSE EXAMPLE_SOURCE_FILES "application/*.cpp")

add_executable(bluetoothModule ${EXAMPLE_SOURCE_FILES})
target_link_libraries(bluetoothModule ${PROJECT_NAME})
set_target_properties(bluetoothModule PROPERTIES LINK_FLAGS "-Wl,-rpath,./lib")

add_subdirectory(cmake)
