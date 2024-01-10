/**
 *  main.c - Application main entry point
 * 
 * This file contains the main code for the application
 * 
 * Author: Nils Lahaye (2024)
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "drivers/adc.h"
#include "drivers/aht20.h"
#include "drivers/ble.h"
#include "drivers/led.h"
#include "drivers/button.h"
#include "utils.h"

LOG_MODULE_REGISTER(MAIN, CONFIG_MAIN_LOG_LEVEL);

/* Struct to store all of the sensors data*/
sensors_data_t sensors_data = {
	.temp = 0,
	.hum = 0,
	.lum = 0,
	.gnd_temp = 0,
	.gnd_hum = 0,
	.bat = 0
};

/* Is it the first run */
bool first_run = true;

/* Sleep time bettwen iterations*/
static uint16_t sleep_time = CONFIG_SENSOR_SLEEP_DURATION_SEC;
/* Time spent sleeping */
static uint16_t sleep_time_spent = 0;

/**
 * @brief Read all of the sensors data and store it in the sensors_data variable
 */
static void read(void) {
		LOG_INF("Reading sensors data");


		// Read the temperature and humidity
		LOG_IF_ERR(aht20_read(&sensors_data.temp, &sensors_data.hum), "Unable to read temperature and humidity");
		// Read the luminosity
		LOG_IF_ERR(luminosity_read(&sensors_data.lum), "Unable to read luminosity");
		// Read the ground temperature
		LOG_IF_ERR(ground_temperature_read(&sensors_data.gnd_temp), "Unable to read ground temperature");
		// Read the ground humidity
		LOG_IF_ERR(ground_humidity_read(&sensors_data.gnd_hum), "Unable to read ground humidity");
		// Read the battery level
		LOG_IF_ERR(battery_voltage_read(&sensors_data.bat), "Unable to read battery level");

		LOG_INF("All sensors data read");
}

/**
 * @brief Send the sensors data to the gateway (Bluetooth)
 */
static void send(void) {
	LOG_INF("Sending sensors data");

	// Encode the data into the service data of the ble driver
	LOG_IF_ERR(ble_encode_adv_data(&sensors_data), "Unable to encode data");

	// Advertise the data
	LOG_IF_ERR(ble_adv(), "Unable to advertise data");

	LOG_INF("Sensors data sent");
}

/**
 * @brief Main function
 */
void main(void) {
	LOG_INF("Starting application");

	// Initialize the LED driver
	LOG_IF_ERR(led_init(), "Unable to initialize LED");
	// Initialize the button driver
	LOG_IF_ERR(button_init(), "Unable to initialize button");
	// Initialize the ADC driver
	LOG_IF_ERR(adc_init(), "Unable to initialize ADC");
	// Initialize the AHT20 driver
	LOG_IF_ERR(aht20_init(), "Unable to initialize AHT20");
	// Initialize the BLE driver
	LOG_IF_ERR(ble_init(&sleep_time), "Unable to initialize BLE");

	LOG_INF("All drivers initialized");

	while(true) {
		LOG_INF("Starting a new loop");
		k_sleep(K_SECONDS(1));
		
		// Read the sensors data
		read();

		if(first_run) {
			// Re read the sensors data to avoid sending wrong data (the first read is often wrong)
			LOG_INF("First run, re reading sensors data");

			read();
			first_run = false;
		}

		// Send the sensors data
		send();

		// Wait
		sleep_time_spent = 0;
		LOG_INF("Sleeping for %d seconds", sleep_time);
		while(sleep_time_spent < sleep_time && !get_button1_state()) {
			k_yield();
			k_sleep(K_SECONDS(1));
			sleep_time_spent++;
		}
		if(get_button1_state()) LOG_INF("Button pressed, waking up");
	}
}
