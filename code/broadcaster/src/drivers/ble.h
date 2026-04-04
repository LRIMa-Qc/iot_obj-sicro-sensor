/**
 * ble.h
 *
 * BLE driver header file
 *
 * Author: Nils Lahaye (2024)
 *
 */

#ifndef BLE_H_
#define BLE_H_

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "../utils.h"
#include "button.h"
#include "led.h"

#define TEMP_ID 1
#define HUM_ID 2
#define CO2_ID 6
#define LUM_ID 3
#define GND_TEMP_ID 4
#define GND_HUM_ID 5
#define BAT_ID 254

#define DEVICE_NAME CONFIG_BLE_USER_DEFINED_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

/**
 * @brief BLE state machine definition
 *
 * State transitions:
 *   IDLE → ADVERTISING (on ble_adv() call)
 *   ADVERTISING → IDLE (on adv timer or disconnect)
 *   ADVERTISING → CONNECTED (on connection)
 *   CONNECTED → DISCONNECT (on timeout or user disconnect)
 *   DISCONNECT → IDLE (automatically, awaited by ble_adv())
 *   TIMEOUT → DISCONNECT (automatically, on inactivity timeout)
 */
typedef enum {
	BLE_STATE_IDLE = 0,           /**< No advertising, not connected */
	BLE_STATE_ADVERTISING = 1,    /**< Advertising active */
	BLE_STATE_CONNECTED = 2,      /**< Device connected */
	BLE_STATE_TIMEOUT = 3,        /**< Connection timed out, awaiting disconnect */
	BLE_STATE_DISCONNECT = 4      /**< Disconnect in progress */
} ble_state_t;

#define BROADCAST_SERVICE_UUID_1 0xab
#define BROADCAST_SERVICE_UUID_2 0xcd

#if CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED

#define SLEEP_TIME_SERVICE_UUID 0xAFBE
#define SLEEP_TIME_CHARACTERISTIC_UUID 0xFAEB

#endif

/**
 * @brief Initialize the BLE driver
 *
 * @param sleep_time_ptr Pointer to the sleep time variable
 * @return int 0 if no error, error code otherwise
 */
int ble_init(uint16_t *sleep_time_ptr);

/**
 * @brief Encode the data into the service data
 *
 * @param sensors_data data to encode
 *
 * @return int 0 if no error, error code otherwise
 */
int ble_encode_adv_data(sensors_data_t *sensors_data);

/**
 * @brief Start the advertising(s) process
 *
 * @return int 0 if no error, error code otherwise
 */
int ble_adv(void);

/**
 * @brief Stop the advertising(s) process
 *
 * @return int 0 if no error, error code otherwise
 */
int ble_stop_adv(void);

/**
 * @brief Transition BLE state machine to a new state
 *
 * Validates the transition and performs any necessary setup/cleanup.
 * Invalid transitions return an error code.
 *
 * @param new_state Target state to transition to
 * @return int 0 if successful, negative error code on invalid transition
 */
int ble_transition_to_state(ble_state_t new_state);

/**
 * @brief Get current BLE state
 *
 * @return ble_state_t Current state of the BLE state machine
 */
ble_state_t ble_get_state(void);

#endif /* BLE_H_ */