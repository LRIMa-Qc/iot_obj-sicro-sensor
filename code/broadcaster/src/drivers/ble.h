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

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>
#include "../utils.h"


#define TEMP_ID 1
#define HUM_ID 2
#define LUM_ID 3
#define GND_TEMP_ID 4
#define GND_HUM_ID 5
#define BAT_ID 254

#define BROADCAST_DEVICE_NAME CONFIG_BLE_USER_DEFINED_BROADCAST_NAME
#define BROADCAST_DEVICE_NAME_LEN (sizeof(BROADCAST_DEVICE_NAME) - 1)
#define BROADCAST_DEVICE_ID 0

#define BROADCAST_SERVICE_UUID_1 0xab
#define BROADCAST_SERVICE_UUID_2 0xcd

#if CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED

    #define CONN_DEVICE_NAME CONFIG_BLE_USER_DEFINED_CONN_NAME
    #define CONN_DEVICE_NAME_LEN (sizeof(CONN_DEVICE_NAME) - 1)
    #define CONN_DEVICE_ID 1

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

#endif /* BLE_H_ */