/**
 * sht4x.h
 *
 * SHT4X driver wrapper using Zephyr sensor API.
 *
 * Author: Nils Lahaye (2026)
 */

#ifndef SHT4X_H_
#define SHT4X_H_

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include "../utils.h"

int sht4x_init(void);

int sht4x_read(float *temperature, float *humidity);

#endif /* SHT4X_H_ */
