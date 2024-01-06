/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "drivers/adc.h"
#include "drivers/sht20.h"
#include "drivers/aht20.h"
#include "drivers/ble.h"
#include "utils.h"

LOG_MODULE_REGISTER(MAIN, CONFIG_MAIN_LOG_LEVEL);

// List of available humidy/temperature I2C devices
static temp_hum_i2c_device available_temp_hum_i2c_devices[] = {
	{"AHT20", 0x38, aht20_init, aht20_read},
	{"SHT20", 0x40, sht20_init, sht20_read},
};

temp_hum_i2c_device current_temp_hum_i2c_device = {
	.label = "",
	.address = 0,
	.initFuncPtr = NULL,
	.readFuncPtr = NULL};

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
 * @brief Test every available I2C devices then select the first one that works
 *
 */
static void scan_i2c_devices(void)
{
	LOG_INF("Scanning I2C devices");

	for (int i = 0; i < ARRAY_SIZE(available_temp_hum_i2c_devices); i++)
	{
		// Get the device
		const struct device *dev = device_get_binding(available_temp_hum_i2c_devices[i].label);

		LOG_INF("Scanning device %s", available_temp_hum_i2c_devices[i].label);
		if (!dev)
		{
			LOG_ERR("I2C: Device driver not found.");
			continue;
		}
		// Check if the device is ready
		RET_IF_ERR(!device_is_ready(dev), "I2C device not ready");

		// Check if the device is responding

		// Send a basic command to the device
		struct i2c_msg cmdBuff[1];
		uint8_t dst;
		// uint8_t cmdBuffer[4];       /* Command buffer */

		// i2c_write(dev, cmdBuff, 1, available_temp_hum_i2c_devices[i].address); // prob switch to that, cant test it

		cmdBuff[0].buf = &dst;
		cmdBuff[0].len = 0U;
		cmdBuff[0].flags = I2C_MSG_WRITE | I2C_MSG_STOP;
		if (i2c_transfer(dev, &cmdBuff[0], 1, available_temp_hum_i2c_devices[i].address) == 0)
		{
			LOG_INF("Found device %s at 0x%02x", available_temp_hum_i2c_devices[i].label, available_temp_hum_i2c_devices[i].address);
			current_temp_hum_i2c_device = available_temp_hum_i2c_devices[i];
			return;
		}
	}

	if (current_temp_hum_i2c_device.initFuncPtr == NULL)
	{
		LOG_ERR("No temperature/humidity sensor found");
		return;
	}

}

/**
 * @brief Read the sensors data
 */
static void read(void)
{
	LOG_INF("Reading sensors data");

	// Update the last sensors data
	last_sensors_data.temp = sensors_data.temp;
	last_sensors_data.hum = sensors_data.hum;
	last_sensors_data.lum = sensors_data.lum;
	last_sensors_data.gnd_temp = sensors_data.gnd_temp;
	last_sensors_data.gnd_hum = sensors_data.gnd_hum;
	last_sensors_data.bat = sensors_data.bat;

	// Read the temperature and humidity
	RET_IF_ERR(current_temp_hum_i2c_device.readFuncPtr(&sensors_data.temp, &sensors_data.hum), "Unable to read temperature and humidity");
	// RET_IF_ERR(sht20_read(&sensors_data.temp, &sensors_data.hum), "Unable to read temperature and humidity");
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
static bool should_send(void)
{
	// Check if the data has changed
	if (sensors_data.temp != last_sensors_data.temp ||
		sensors_data.hum != last_sensors_data.hum ||
		sensors_data.lum != last_sensors_data.lum ||
		sensors_data.gnd_temp != last_sensors_data.gnd_temp ||
		sensors_data.gnd_hum != last_sensors_data.gnd_hum ||
		sensors_data.bat != last_sensors_data.bat)
	{
		LOG_INF("Sensors data has changed");
		return true;
	}

	LOG_INF("Sensors data has not changed");

	return false;
}

/**
 * @brief Send the sensors data
 */
static void send(void)
{
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
void main(void)
{
	LOG_INF("Starting application");

	// Initialize the ADC driver
	RET_IF_ERR(adc_init(), "Unable to initialize ADC");
	// Scan the I2C devices
	scan_i2c_devices();
	// Initialize the Temperature/Humidity driver
	RET_IF_ERR(current_temp_hum_i2c_device.initFuncPtr(), "Unable to initialize Temperature/Humidity sensor");
	// RET_IF_ERR(sht20_init(), "Unable to initialize SHT20");
	// Initialize the BLE driver
	RET_IF_ERR(ble_init(), "Unable to initialize BLE");

	LOG_INF("All drivers initialized");

	while (true)
	{
		LOG_INF("Starting a new loop");

		// Read the sensors data
		read();

		if (first_run)
		{
			// Re read the sensors data to avoid sending wrong data
			LOG_INF("First run, re reading sensors data");

			read();
			first_run = false;
		}

		// Send the sensors data if needed
		LOG_INF("Checking if the sensors data should be sent");
		if (should_send())
			send();
		else
			LOG_INF("Sensors data will not be sent");

		// Wait
		LOG_INF("Sleeping for %d seconds", CONFIG_SENSOR_SLEEP_DURATION_SEC);
		k_sleep(K_SECONDS(CONFIG_SENSOR_SLEEP_DURATION_SEC));
	}
}
