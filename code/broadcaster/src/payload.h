/**
 * payload.h
 *
 * Payload encoding module for BLE advertisement data
 *
 * Encodes sensor data into a standardized 29-byte service data format.
 * Author: Nils Lahaye (2024)
 */

#ifndef PAYLOAD_H_
#define PAYLOAD_H_

#include "utils.h"
#include <stddef.h>
#include <stdint.h>


#ifndef BIT
#define BIT(n) (1U << (n))
#endif

#define PAYLOAD_SIZE 29

#define PAYLOAD_COMPANY_ID 0x0059
#define PAYLOAD_PRESENT_TEMP BIT(0)
#define PAYLOAD_PRESENT_HUM BIT(1)
#define PAYLOAD_PRESENT_LUM BIT(2)
#define PAYLOAD_PRESENT_CO2 BIT(3)
#define PAYLOAD_PRESENT_GND_TEMP BIT(4)
#define PAYLOAD_PRESENT_GND_HUM BIT(5)
#define PAYLOAD_PRESENT_BAT BIT(6)

/**
 * @brief Encode sensor data into BLE advertisement payload
 *
 * Packs sensor readings into a 29-byte service data format.
 *
 * **Payload Format (29 bytes total):**
 * - Bytes 0-1: Company ID (0x0059, little-endian)
 * - Bytes 2-3: Node ID (uint16_t, little-endian)
 * - Byte 4: Sequence counter (uint8_t)
 * - Bytes 5-8: Current sleep duration in seconds (uint32_t)
 * - Bytes 9-10: Air temperature (int16_t, 0.01 °C)
 * - Bytes 11-12: Air humidity (uint16_t, 0.01 %RH)
 * - Bytes 13-16: Luminosity (uint32_t, lux)
 * - Bytes 17-18: CO2 (uint16_t, ppm)
 * - Bytes 19-20: Ground temperature (int16_t, 0.01 °C)
 * - Bytes 21-22: Ground humidity (uint16_t, 0.01 %RH)
 * - Bytes 23-24: Battery (uint16_t, 0.01 V)
 * - Byte 25: Presence bitmap
 * - Bytes 26-28: Reserved, set to 0x00
 *
 * @param data Pointer to sensor data structure
 * @param buf Output buffer (must be at least 25 bytes)
 * @param buf_len Length of output buffer
 * @param present_mask Bitmap describing which sensor fields are valid
 * @return 0 on success, negative error code on failure
 */
int payload_encode(const sensors_data_t *data, uint8_t *buf, size_t buf_len,
                   uint8_t present_mask, uint32_t sleep_duration_sec);

/**
 * @brief Get the size of the encoded payload
 *
 * @return size_t Payload size in bytes (always 29)
 */
size_t payload_get_size(void);

#endif /* PAYLOAD_H_ */
