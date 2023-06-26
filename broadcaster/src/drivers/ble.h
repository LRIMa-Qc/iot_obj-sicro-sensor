/**
 * ble.h
 * 
 * BLE driver header file
 * 
 * Author: Nils Lahahye (2023)
 * 
*/

#ifndef BLE_H_
#define BLE_H_

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/logging/log.h>
#include "../utils.h"


#define TEMP_ID 1
#define HUM_ID 2
#define LUM_ID 3
#define GND_TEMP_ID 4
#define GND_HUM_ID 5
#define BAT_ID 254

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define SERVICE_UUID_1 0xab
#define SERVICE_UUID_2 0xcd

int ble_init(void);

int ble_encode_adv_data(sensors_data_t *sensors_data);

int ble_adv(void);

static void ble_adv_start(int err);

#endif /* BLE_H_ */