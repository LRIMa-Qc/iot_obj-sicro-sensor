/**
 * payload.c
 *
 * Payload encoding module for BLE advertisement data
 *
 * Author: Nils Lahaye (2024)
 */

#include "payload.h"
#include "utils.h"
#include <errno.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(PAYLOAD, CONFIG_LOG_DEFAULT_LEVEL);

#define PAYLOAD_SIZE 25

/* Service UUID and sensor IDs (matching ble.h constants) */
#define BROADCAST_SERVICE_UUID_1 0xab
#define BROADCAST_SERVICE_UUID_2 0xcd

#define TEMP_ID 1
#define HUM_ID 2
#define CO2_ID 6
#define LUM_ID 3
#define GND_TEMP_ID 4
#define GND_HUM_ID 5
#define BAT_ID 254

/**
 * @brief Encode a sensor pair (ID, whole, decimal) into payload
 *
 * @param pos Position in payload buffer (should be 4-22, in 3-byte increments)
 * @param id Sensor identifier
 * @param val Pointer to float value to encode
 * @param buf Payload buffer
 * @return 0 on success, negative error code on failure
 */
static int encode_pair(uint8_t pos, uint8_t id, const float *val, uint8_t *buf) {
	uint8_t whole, decimal;

	if (floatSeparator((float *)val, &whole, &decimal) != 0) {
		LOG_WRN("Unable to separate float value for sensor %u", id);
		return -EINVAL;
	}

	/* Encode sign in decimal value (negative values: decimal += 100) */
	if (*val < 0) {
		whole = whole * -1;  /* Negate whole to indicate sign */
		decimal += 100;
	}

	buf[pos] = id;
	buf[pos + 1] = whole;
	buf[pos + 2] = decimal;

	return 0;
}

/**
 * @brief Encode sensor data into BLE advertisement payload
 *
 * @param data Pointer to sensor data structure
 * @param buf Output buffer
 * @param buf_len Buffer length in bytes
 * @return 0 on success, negative error code on failure
 */
int payload_encode(const sensors_data_t *data, uint8_t *buf, size_t buf_len) {
	if (!data || !buf) {
		LOG_ERR("Invalid pointer: data=%p, buf=%p", data, buf);
		return -EINVAL;
	}

	if (buf_len < PAYLOAD_SIZE) {
		LOG_ERR("Buffer too small: %zu bytes (need %u)", buf_len, PAYLOAD_SIZE);
		return -ENOBUFS;
	}

	/* Service UUID (set by caller - just verify we have space) */
	/* The caller should set bytes 0-1 with the UUID */
	/* Here we'll leave bytes 0-1 as-is (caller responsibility) */

	/* Encode each sensor as a 3-byte pair */
	if (encode_pair(4, TEMP_ID, &data->temp, buf) != 0) {
		LOG_ERR("Failed to encode temperature");
		return -EIO;
	}

	if (encode_pair(7, HUM_ID, &data->hum, buf) != 0) {
		LOG_ERR("Failed to encode humidity");
		return -EIO;
	}

	/* CO2 is encoded as ppm/10 to fit the format */
	float co2_div10 = data->co2 / 10.0f;
	if (encode_pair(10, CO2_ID, &co2_div10, buf) != 0) {
		LOG_ERR("Failed to encode CO2");
		return -EIO;
	}

	if (encode_pair(13, LUM_ID, &data->lum, buf) != 0) {
		LOG_ERR("Failed to encode luminosity");
		return -EIO;
	}

	if (encode_pair(16, GND_TEMP_ID, &data->gnd_temp, buf) != 0) {
		LOG_ERR("Failed to encode ground temperature");
		return -EIO;
	}

	if (encode_pair(19, GND_HUM_ID, &data->gnd_hum, buf) != 0) {
		LOG_ERR("Failed to encode ground humidity");
		return -EIO;
	}

	if (encode_pair(22, BAT_ID, &data->bat, buf) != 0) {
		LOG_ERR("Failed to encode battery");
		return -EIO;
	}

	LOG_DBG("Payload encoded successfully (%u bytes)", PAYLOAD_SIZE);

	return 0;
}

/**
 * @brief Get the size of the encoded payload
 *
 * @return size_t Payload size in bytes (always 25)
 */
size_t payload_get_size(void) {
	return PAYLOAD_SIZE;
}
