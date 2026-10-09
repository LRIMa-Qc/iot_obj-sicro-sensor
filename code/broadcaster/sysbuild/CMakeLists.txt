# SPDX-License-Identifier: Apache-2.0

# Sysbuild entry point for this application.
#
# Board discovery for sysbuild happens inside find_package(Sysbuild), which is
# evaluated before an application's top-level sysbuild.cmake is processed.
# Register the in-tree board root here so the custom board is found.
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

find_package(Sysbuild REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(sysbuild LANGUAGES)
