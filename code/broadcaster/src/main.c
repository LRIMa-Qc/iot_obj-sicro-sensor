/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "drivers/adc.h"
#include "drivers/aht20.h"
#include "drivers/ble.h"
#include "utils.h"

LOG_MODULE_REGISTER(MAIN, CONFIG_MAIN_LOG_LEVEL);

sensors_data_t sensors_data = {
	.temp = 0,
	.hum = 0,
	.lum = 0,
	.gnd_temp = 0,
	.gnd_hum = 0,
	.bat = 0
};

sensors_data_t last_sensors_data = {
	.temp = 0,
	.hum = 0,
	.lum = 0,
	.gnd_temp = 0,
	.gnd_hum = 0,
	.bat = 0
};

bool first_run = true;

/**
 * @brief Read the sensors data
 */
static void read(void) {
		LOG_INF("Reading sensors data");

		// Update the last sensors data
		last_sensors_data.temp = sensors_data.temp;
		last_sensors_data.hum = sensors_data.hum;
		last_sensors_data.lum = sensors_data.lum;
		last_sensors_data.gnd_temp = sensors_data.gnd_temp;
		last_sensors_data.gnd_hum = sensors_data.gnd_hum;
		last_sensors_data.bat = sensors_data.bat;

		// Read the temperature and humidity
		RET_IF_ERR(aht20_read(&sensors_data.temp, &sensors_data.hum), "Unable to read temperature and humidity");
		// Read the luminosity
		RET_IF_ERR(luminosity_read(&sensors_data.lum), "Unable to read luminosity");
		// Read the ground temperature
		RET_IF_ERR(ground_temperature_read(&sensors_data.gnd_temp), "Unable to read ground temperature");
		// Read the ground humidity
		RET_IF_ERR(ground_humidity_read(&sensors_data.gnd_hum), "Unable to read ground humidity");
		// Read the battery level
		RET_IF_ERR(battery_voltage_read(&sensors_data.bat), "Unable to read battery level");

		LOG_INF("All sensors data read");
}

/**
 * @brief Check if the sensors data should be sent
 * @return true if the sensors data should be sent, false otherwise
 */
static bool should_send(void) {
	// Check if the data has changed
	if(sensors_data.temp != last_sensors_data.temp ||
		sensors_data.hum != last_sensors_data.hum ||
		sensors_data.lum != last_sensors_data.lum ||
		sensors_data.gnd_temp != last_sensors_data.gnd_temp ||
		sensors_data.gnd_hum != last_sensors_data.gnd_hum ||
		sensors_data.bat != last_sensors_data.bat) {
			LOG_INF("Sensors data has changed");
			return true;
	}

	LOG_INF("Sensors data has not changed");

	return false;
}

/**
 * @brief Send the sensors data
 */
static void send(void) {
	LOG_INF("Sending sensors data");

	// Encode the data into the service data
	RET_IF_ERR(ble_encode_adv_data(&sensors_data), "Unable to encode data");

	// Advertise the data
	RET_IF_ERR(ble_adv(), "Unable to advertise data");

	LOG_INF("Sensors data sent");
}

/**
 * @brief Main function
 */
void main(void) {
	LOG_INF("Starting application");

	// Initialize the ADC driver
	RET_IF_ERR(adc_init(), "Unable to initialize ADC");
	// Initialize the AHT20 driver
	RET_IF_ERR(aht20_init(), "Unable to initialize AHT20");
	// Initialize the BLE driver
	RET_IF_ERR(ble_init(), "Unable to initialize BLE");

	LOG_INF("All drivers initialized");

	while(true) {
		LOG_INF("Starting a new loop");
		
		// Read the sensors data
		read();

		if(first_run) {
			// Re read the sensors data to avoid sending wrong data
			LOG_INF("First run, re reading sensors data");

			read();
			first_run = false;
		}

		// Send the sensors data if needed
		LOG_INF("Checking if the sensors data should be sent");
		if(should_send()) send();
		else LOG_INF("Sensors data will not be sent");

		// Wait
		LOG_INF("Sleeping for %d seconds", CONFIG_SENSOR_SLEEP_DURATION_SEC);
		k_sleep(K_SECONDS(CONFIG_SENSOR_SLEEP_DURATION_SEC));
	}
}
