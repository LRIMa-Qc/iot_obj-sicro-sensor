/**
 * payload.h
 *
 * Payload encoding module for BLE advertisement data
 *
 * Encodes sensor data into a standardized 25-byte service data format.
 * Author: Nils Lahaye (2024)
 */

#ifndef PAYLOAD_H_
#define PAYLOAD_H_

#include <stddef.h>
#include <stdint.h>
#include "utils.h"

/**
 * @brief Encode sensor data into BLE advertisement payload
 *
 * Packs sensor readings into a 25-byte service data format.
 *
 * **Payload Format (25 bytes total):**
 * - Bytes 0-1: Service UUID (0xABCD)
 * - Bytes 2-3: Packet counter (uint16_t, little-endian)
 * - Bytes 4-6:   Temperature    [ID=1] [whole] [decimal]
 * - Bytes 7-9:   Humidity       [ID=2] [whole] [decimal]
 * - Bytes 10-12: CO2            [ID=6] [whole/10] [decimal]
 * - Bytes 13-15: Luminosity     [ID=3] [whole] [decimal]
 * - Bytes 16-18: Ground Temp    [ID=4] [whole] [decimal]
 * - Bytes 19-21: Ground Humidity[ID=5] [whole] [decimal]
 * - Bytes 22-24: Battery        [ID=254][whole] [decimal]
 *
 * **Value Encoding (3 bytes per sensor):**
 * - Each float is decomposed into whole and decimal parts
 * - Negative values: decimal += 100 to encode sign
 *   Example: -5.3 → whole=255 (0xFF), decimal=103 (5 + 100, sign in whole)
 *   Example: 23.5 → whole=23, decimal=50
 * - CO2 is pre-divided by 10 before encoding to fit the format
 *   Example: 2000 ppm → 200.0 → whole=200, decimal=0
 *
 * @param data Pointer to sensor data structure
 * @param buf Output buffer (must be at least 25 bytes)
 * @param buf_len Length of output buffer
 * @return 0 on success, negative error code on failure
 */
int payload_encode(const sensors_data_t *data, uint8_t *buf, size_t buf_len);

/**
 * @brief Get the size of the encoded payload
 *
 * @return size_t Payload size in bytes (always 25)
 */
size_t payload_get_size(void);

#endif /* PAYLOAD_H_ */
