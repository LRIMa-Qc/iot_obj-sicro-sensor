/**
 *  main.c - Application main entry point
 * 
 * This file contains the main code for the application
 * 
 * Author: Nils Lahaye (2024)
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/reboot.h>
#include <string.h>
#include "drivers/adc.h"
#include "drivers/aht20.h"
#include "drivers/sht20.h"
#include "drivers/sht31.h"
#include "drivers/sht4x.h"
#include "drivers/stcc4.h"
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
	.co2 = 0,
	.lum = 0,
	.gnd_temp = 0,
	.gnd_hum = 0,
	.bat = 0
};

/* Is it the first run */
bool first_run = true;

/* Sleep time bettwen iterations*/
static uint16_t sleep_time = CONFIG_SENSOR_SLEEP_DURATION_SEC;
static uint16_t persisted_sleep_time = CONFIG_SENSOR_SLEEP_DURATION_SEC;
static uint64_t initial_timestamp = 0;

//Pointer to the current i2c temperature and humidity sensor
static int (*ptr_temp_hum_read)(float *, float *);
// Pointer to optional co2 sensor read function
static int (*ptr_co2_read)(float *);

static uint16_t clamp_sleep_time_value(uint16_t value) {
	if (value < 1U) {
		return 1U;
	}

#if CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED
	if (value > CONFIG_SENSOR_SLEEP_DURATION_MAX_SEC) {
		return CONFIG_SENSOR_SLEEP_DURATION_MAX_SEC;
	}
#endif

	return value;
}

static int sleep_time_settings_set(const char *name, size_t len_rd,
					   settings_read_cb read_cb, void *cb_arg) {
	uint16_t loaded_value;

	if (strcmp(name, "sleep_time") != 0) {
		return -ENOENT;
	}

	if (len_rd != sizeof(loaded_value)) {
		LOG_WRN("Invalid stored sleep_time size: %u", (unsigned int)len_rd);
		return -EINVAL;
	}

	int rc = read_cb(cb_arg, &loaded_value, sizeof(loaded_value));
	if (rc < 0) {
		LOG_ERR("Unable to read stored sleep_time (%d)", rc);
		return rc;
	}

	sleep_time = clamp_sleep_time_value(loaded_value);
	persisted_sleep_time = sleep_time;

	LOG_INF("Loaded persisted sleep_time: %u", sleep_time);

	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(app_settings, "app", NULL,
			       sleep_time_settings_set, NULL, NULL);

static int load_sleep_time_settings(void) {
	RET_IF_ERR(settings_subsys_init(), "Unable to initialize settings subsystem");

	int rc = settings_load_subtree("app");
	if (rc) {
		LOG_ERR("Unable to load app settings (%d)", rc);
		return rc;
	}

	return 0;
}

static int save_sleep_time_settings(void) {
	if (sleep_time == persisted_sleep_time) {
		LOG_DBG("Sleep time unchanged, skipping persistence (%u sec)", sleep_time);
		return 0;
	}

	LOG_INF("Sleep time changed in RAM: %u -> %u sec", persisted_sleep_time, sleep_time);

	sleep_time = clamp_sleep_time_value(sleep_time);
	RET_IF_ERR(settings_save_one("app/sleep_time", &sleep_time, sizeof(sleep_time)),
		   "Unable to save sleep_time setting");

	persisted_sleep_time = sleep_time;
	LOG_INF("Persisted sleep_time: %u", sleep_time);
	/* Verify that the persisted value matches in-memory value */
	if (persisted_sleep_time != sleep_time) {
		LOG_WRN("Persistence verification failed: persisted=%u, in-memory=%u",
			persisted_sleep_time, sleep_time);
		return -EIO;
	}

	return 0;
}

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
	if (ptr_co2_read != NULL) { LOG_IF_ERR(ptr_co2_read(&sensors_data.co2), "Unable to read co2"); }
	else sensors_data.co2 = 0;
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

	ptr_co2_read = NULL;

	// Try to initialize the STCC4 sensor
	if(!stcc4_init()) {
		LOG_INF("STCC4 sensor initialized");
		ptr_temp_hum_read = &stcc4_read;
		ptr_co2_read = &stcc4_read_co2;
		return 0;
	}
	// Try to initialize the SHT4X sensor
	if(!sht4x_init()) {
		LOG_INF("SHT4X sensor initialized");
		ptr_temp_hum_read = &sht4x_read;
		return 0;
	}

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
	ptr_co2_read = NULL;
	return 1;
}

/**
 * @brief Main function
 */
int main(void) {
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
	// Load the persisted sleep time setting
	LOG_IF_ERR(load_sleep_time_settings(), "Unable to load sleep time settings");

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

		LOG_IF_ERR(save_sleep_time_settings(), "Unable to persist sleep time");

		LOG_INF("Sleeping for %u seconds", sleep_time);
		int sem_result = k_sem_take(button_get_pressed_sem(), K_SECONDS(sleep_time));
		if (sem_result == 0) {
			LOG_INF("Button pressed, waking up");
		}
	}

	return 0;
}
