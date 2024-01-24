/**
 * sht31.h
 * 
 * This file is used to define the SHT31 sensor constants and functions.
 * 
 * Author: Nils Lahaye (2024)
 * 
*/

#ifndef SHT31_H_
#define SHT31_H_

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h> 
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include "../utils.h"

int sht31_init(void);

int sht31_read(float *temperature, float *humidity);

#endif /* SHT31_H */