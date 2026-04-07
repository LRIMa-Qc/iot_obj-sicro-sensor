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

#define BLE_STRINGIFY_IMPL(x) #x
#define BLE_STRINGIFY(x) BLE_STRINGIFY_IMPL(x)
#define BLE_NAME CONFIG_BLE_USER_DEFINED_NAME " " BLE_STRINGIFY(CONFIG_BLE_NODE_ID)

/**
 * @brief BLE state machine definition
 *
 * State transitions:
 *   IDLE → ADVERTISING (on ble_adv() call)
 *   ADVERTISING → IDLE (on adv timer or stop)
 */
typedef enum {
  BLE_STATE_IDLE = 0,        /**< No advertising, not connected */
  BLE_STATE_ADVERTISING = 1, /**< Advertising active */
  BLE_STATE_SCANNING = 2     /**< Active scan window for downlink */
} ble_state_t;

#define BROADCAST_SERVICE_UUID_1 0xab
#define BROADCAST_SERVICE_UUID_2 0xcd

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
int ble_encode_adv_data(sensors_data_t *sensors_data, uint8_t present_mask,
                        uint32_t sleep_duration_sec);

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

/**
 * @brief Fetch pending sleep update from BLE downlink path.
 *
 * @param sleep_time_sec_out Output pointer receiving the pending sleep value
 * @return true when a new value was available and copied, false otherwise
 */
bool ble_take_pending_sleep_update(uint16_t *sleep_time_sec_out);

#endif /* BLE_H_ */