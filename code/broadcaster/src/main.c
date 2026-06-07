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
#include "drivers/sht4x.h"
#include "drivers/stcc4.h"
#include "drivers/ble.h"
#include "drivers/led.h"
#include "drivers/button.h"
#include "payload.h"
#include "utils.h"
#include "utils/app_settings.h"

#define WEEK_IN_MILLISECONDS (7 * 24 * 60 * 60 * 1000)
#define INIT_REBOOT_DELAY_SECONDS 5

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
static uint64_t initial_timestamp = 0;

#define INIT_OR_REBOOT(_init_call, _name)                  \
	do {                                                     \
		int _err = (_init_call);                              \
		if (_err) {                                            \
			init_fail_and_reboot((_name), _err);                \
		}                                                      \
	} while (0)

//Pointer to the current i2c temperature and humidity sensor
static int (*ptr_temp_hum_read)(float *, float *);
// Pointer to optional co2 sensor read function
static int (*ptr_co2_read)(float *);
// Pointer to combined temperature, humidity and co2 sensor read function
static int (*ptr_temp_hum_co2_read)(float *, float *, float *);

static void init_fail_and_reboot(const char *name, int err) {
	LOG_ERR("Unable to initialize %s (err: %d)", name, err);
	(void)led_blink_stop();
	(void)led1_on();
	k_sleep(K_SECONDS(INIT_REBOOT_DELAY_SECONDS));
	sys_reboot(SYS_REBOOT_COLD);
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

	// Read the temperature, humidity and co2 together when the sensor supports it
	if (ptr_temp_hum_co2_read != NULL) {
		LOG_IF_ERR(ptr_temp_hum_co2_read(&sensors_data.temp, &sensors_data.hum, &sensors_data.co2),
			   "Unable to read temperature, humidity and co2");
	} else {
		// Read the temperature and humidity
		if (ptr_temp_hum_read != NULL) {
			LOG_IF_ERR(ptr_temp_hum_read(&sensors_data.temp, &sensors_data.hum), "Unable to read temperature and humidity");
		} else {
			LOG_WRN("No temperature and humidity sensor found");
		}
		if (ptr_co2_read != NULL) {
			LOG_IF_ERR(ptr_co2_read(&sensors_data.co2), "Unable to read co2");
		} else {
			sensors_data.co2 = 0;
		}
	}

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
	uint8_t present_mask = PAYLOAD_PRESENT_LUM | PAYLOAD_PRESENT_GND_TEMP |
			      PAYLOAD_PRESENT_GND_HUM | PAYLOAD_PRESENT_BAT;

	LOG_INF("Sending sensors data");

	if (ptr_temp_hum_read != NULL || ptr_temp_hum_co2_read != NULL) {
		present_mask |= PAYLOAD_PRESENT_TEMP | PAYLOAD_PRESENT_HUM;
	}

	if (ptr_co2_read != NULL || ptr_temp_hum_co2_read != NULL) {
		present_mask |= PAYLOAD_PRESENT_CO2;
	}

	// Encode the data into the service data of the ble driver
	LOG_IF_ERR(ble_encode_adv_data(&sensors_data, present_mask, (uint32_t)sleep_time),
		   "Unable to encode data");

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

	ptr_temp_hum_co2_read = NULL;
	ptr_co2_read = NULL;

	// Try to initialize the STCC4 sensor
	if(!stcc4_init()) {
		LOG_INF("STCC4 sensor initialized");
		ptr_temp_hum_read = NULL;
		ptr_co2_read = NULL;
		ptr_temp_hum_co2_read = &stcc4_read_all;
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
	ptr_temp_hum_co2_read = NULL;
	return 1;
}

/**
 * @brief Main function
 */
int main(void) {
	LOG_INF("Starting application");

	initial_timestamp = k_uptime_get();

	// Initialize the LED driver
	INIT_OR_REBOOT(led_init(), "LED");

	// Blink at 4 blinks per second while initializing remaining drivers
	INIT_OR_REBOOT(led_blink_start(), "LED blink");

	// Initialize the button driver
	INIT_OR_REBOOT(button_init(), "button");
	// Initialize the ADC driver
	INIT_OR_REBOOT(adc_init(), "ADC");
	// Initialize the AHT20 driver
	INIT_OR_REBOOT(init_temp_hum_sensor(), "temperature and humidity sensor");
	// Load and seed persisted app settings (sleep time, name, MAC)
	INIT_OR_REBOOT(app_settings_init(&sleep_time), "app settings");
	// Initialize the BLE driver
	INIT_OR_REBOOT(ble_init(&sleep_time), "BLE");

	INIT_OR_REBOOT(led_blink_stop(), "LED blink");
	LOG_IF_ERR(led1_on(), "Unable to turn on LED after initialization");
	k_sleep(K_SECONDS(1));
	LOG_IF_ERR(led1_off(), "Unable to turn off LED after initialization");

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

		uint16_t downlink_sleep_time = 0U;
		if (ble_take_pending_sleep_update(&downlink_sleep_time)) {
			sleep_time = downlink_sleep_time;
			LOG_INF("Applied downlink sleep update: %u seconds", sleep_time);
		}

		LOG_IF_ERR(app_settings_persist_sleep_time_if_changed(), "Unable to persist sleep time");

		LOG_INF("Sleeping for %u seconds", sleep_time);
		int sem_result = k_sem_take(button_get_pressed_sem(), K_SECONDS(sleep_time));
		if (sem_result == 0) {
			LOG_INF("Button pressed, waking up");
			if (ble_get_state() == BLE_STATE_ADVERTISING) {
				led1_on();
			}
		}
	}

	return 0;
}
