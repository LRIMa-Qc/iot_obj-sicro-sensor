/**
 * adc.h
 * 
 * This file will read the adc values from the sensors
 * 
 * Author: Nils Lahaye 2023
 * 
*/
#ifndef ADC_H_
#define ADC_H_

#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "../utils.h"

int adc_init(void);

int ground_humidity_read(float *humidity);

int ground_temperature_read(float *temperature);

int luminosity_read(float *luminosity);

int battery_voltage_read(float *voltage);

#endif // ADC_H_
