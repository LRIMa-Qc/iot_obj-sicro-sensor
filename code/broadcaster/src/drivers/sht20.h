/**
 * sth20.h
 * 
 * This file is used to define the SHT20 sensor constants and functions.
 * 
 * Author: Francis Mathieu-Gosselin 2024
 * 
*/

#ifndef SHT20_H_
#define SHT20_H_

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h> 
#include <zephyr/sys/crc.h>
#include "../utils.h"

// https://www.mouser.com/datasheet/2/682/Sensirion_Humidity_Sensors_SHT20_Datasheet-1274196.pdf

#define SHT20_TRIGGER_T_MEASURE_HOLD    0xE3 /* Trigger T measurement Hold master (66ms)*/
#define SHT20_TRIGGER_T_MEASURE_NOHOLD  0xF3 /* Trigger T measurement No hold master */
#define SHT20_TRIGGER_RH_MEASURE_HOLD   0xE5 /* Trigger RH measurement Hold master (22ms)*/
#define SHT20_TRIGGER_RH_MEASURE_NOHOLD 0xF5 /* Trigger RH measurement No hold master */

#define SHT20_SOFT_RESET		     	0xFE /* Reset command (15ms)*/ 

int sht20_init(void);

int sht20_read(float *temperature, float *humidity);

#endif /* SHT20_H */