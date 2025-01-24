/**
 *  main.c - Application main entry point
 * 
 * This file contains the main code for the application
 * 
 * Author: Nils Lahaye (2024)
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>
#include "drivers/adc.h"
#include "drivers/aht20.h"
#include "drivers/sht20.h"
#include "drivers/sht31.h"
#include "drivers/ble.h"
#include "drivers/led.h"
#include "drivers/button.h"
#include "utils.h"

#define WEEK_IN_MILLISECONDS (7 * 24 * 60 * 60 * 1000)

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
static uint64_t initial_timestamp = 0;

//Pointer to the current i2c temperature and humidity sensor
static int (*ptr_temp_hum_read)(float *, float *);

static void check_and_reboot_if_week_elapsed(void) {
    uint64_t current_timestamp = k_uptime_get();
    uint64_t elapsed_time = current_timestamp - initial_timestamp;

    if (elapsed_time >= WEEK_IN_MILLISECONDS) {
        LOG_INF("A week has passed! Triggering reboot...");
        sys_reboot(SYS_REBOOT_COLD);
    }
}

/**
 * @brief Read all of the sensors data and store it in the sensors_data variable
 */
static void read(void) {
	LOG_INF("Reading sensors data");

	// Read the temperature and humidity
	if (ptr_temp_hum_read != NULL) { LOG_IF_ERR(ptr_temp_hum_read(&sensors_data.temp, &sensors_data.hum), "Unable to read temperature and humidity"); }
	else LOG_WRN("No temperature and humidity sensor found");
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
 * @brief Initialize the temperature and humidity sensor
 * 
 * @return 0 on success, error code otherwise
 */
static int init_temp_hum_sensor(void) {
	LOG_INF("Initializing temperature and humidity sensor");

	// Sleeping for 1000ms to wait for the sensor to wake up
	k_sleep(K_MSEC(1000));

	// Try to initialize the AHT20 sensor
	if(!aht20_init()) {
		LOG_INF("AHT20 sensor initialized");
		ptr_temp_hum_read = &aht20_read;
		return 0;
	}
	// Try to initialize the SHT20 sensor
	if(!sht20_init()) {
		LOG_INF("SHT20 sensor initialized");
		ptr_temp_hum_read = &sht20_read;
		return 0;
	}
	// Try to initialize the SHT31 sensor
	if(!sht31_init()) {
		LOG_INF("SHT31 sensor initialized");
		ptr_temp_hum_read = &sht31_read;
		return 0;
	}

	// No sensor found
	LOG_WRN("No temperature and humidity sensor found");
	ptr_temp_hum_read = NULL;
	return 1;
}

/**
 * @brief Main function
 */
void main(void) {
	LOG_INF("Starting application");

	initial_timestamp = k_uptime_get();

	// Initialize the LED driver
	LOG_IF_ERR(led_init(), "Unable to initialize LED");
	// Initialize the button driver
	LOG_IF_ERR(button_init(), "Unable to initialize button");
	// Initialize the ADC driver
	LOG_IF_ERR(adc_init(), "Unable to initialize ADC");
	// Initialize the AHT20 driver
	LOG_IF_ERR(init_temp_hum_sensor(), "Unable to initialize temperature and humidity sensor");
	// Initialize the BLE driver
	LOG_IF_ERR(ble_init(&sleep_time), "Unable to initialize BLE");

	LOG_INF("All drivers initialized");

	while(true) {
		LOG_INF("Starting a new loop");
		check_and_reboot_if_week_elapsed();

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
