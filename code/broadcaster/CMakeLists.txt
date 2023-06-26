# SPDX-License-Identifier: Apache-2.0
# Nils Lahaye 2023

cmake_minimum_required(VERSION 3.20.0)

# Required files

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(broadcaster)

# Add util source file
SET(UTILS_H
    src/utils.h
    )
SET(UTILS_C
    src/utils.c
    )

# Add drivers source files
SET(DRIVERS_H
    src/drivers/aht20.h
    src/drivers/adc.h
    src/drivers/ble.h
    )
SET(DRIVERS_C
    src/drivers/aht20.c
    src/drivers/adc.c
    src/drivers/ble.c
    )


# Add sources as target
target_sources(app PRIVATE ${UTILS_H} ${UTILS_C})
target_sources(app PRIVATE ${DRIVERS_H} ${DRIVERS_C})
target_sources(app PRIVATE src/main.c)

