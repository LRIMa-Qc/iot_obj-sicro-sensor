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
#include <string.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(PAYLOAD, CONFIG_LOG_DEFAULT_LEVEL);

#define COMPANY_ID_LE PAYLOAD_COMPANY_ID

static uint8_t clamp_present_mask(uint8_t present_mask)
{
	return present_mask & (PAYLOAD_PRESENT_TEMP | PAYLOAD_PRESENT_HUM | PAYLOAD_PRESENT_LUM |
			      PAYLOAD_PRESENT_CO2 | PAYLOAD_PRESENT_GND_TEMP |
			      PAYLOAD_PRESENT_GND_HUM | PAYLOAD_PRESENT_BAT);
}

static void put_u16(uint8_t *buf, size_t offset, uint16_t value)
{
	buf[offset] = (uint8_t)(value & 0xFFU);
	buf[offset + 1U] = (uint8_t)((value >> 8) & 0xFFU);
}

static void put_i16(uint8_t *buf, size_t offset, int16_t value)
{
	put_u16(buf, offset, (uint16_t)value);
}

static void put_u32(uint8_t *buf, size_t offset, uint32_t value)
{
	buf[offset] = (uint8_t)(value & 0xFFU);
	buf[offset + 1U] = (uint8_t)((value >> 8) & 0xFFU);
	buf[offset + 2U] = (uint8_t)((value >> 16) & 0xFFU);
	buf[offset + 3U] = (uint8_t)((value >> 24) & 0xFFU);
}

int payload_encode(const sensors_data_t *data, uint8_t *buf, size_t buf_len, uint8_t present_mask,
		   uint32_t sleep_duration_sec)
{
	if (!data || !buf) {
		LOG_ERR("Invalid pointer: data=%p, buf=%p", data, buf);
		return -EINVAL;
	}

	if (buf_len < PAYLOAD_SIZE) {
		LOG_ERR("Buffer too small: %zu bytes (need %u)", buf_len, PAYLOAD_SIZE);
		return -ENOBUFS;
	}

	memset(&buf[5], 0, PAYLOAD_SIZE - 5);
	present_mask = clamp_present_mask(present_mask);
	put_u32(buf, 5, sleep_duration_sec);

	if (present_mask & PAYLOAD_PRESENT_TEMP) {
		put_i16(buf, 9, (int16_t)lroundf(data->temp * 100.0f));
	}

	if (present_mask & PAYLOAD_PRESENT_HUM) {
		put_u16(buf, 11, (uint16_t)lroundf(data->hum * 100.0f));
	}

	if (present_mask & PAYLOAD_PRESENT_LUM) {
		put_u32(buf, 13, (uint32_t)lroundf(data->lum));
	}

	if (present_mask & PAYLOAD_PRESENT_CO2) {
		put_u16(buf, 17, (uint16_t)lroundf(data->co2));
	}

	if (present_mask & PAYLOAD_PRESENT_GND_TEMP) {
		put_i16(buf, 19, (int16_t)lroundf(data->gnd_temp * 100.0f));
	}

	if (present_mask & PAYLOAD_PRESENT_GND_HUM) {
		put_u16(buf, 21, (uint16_t)lroundf(data->gnd_hum * 100.0f));
	}

	if (present_mask & PAYLOAD_PRESENT_BAT) {
		put_u16(buf, 23, (uint16_t)lroundf(data->bat * 100.0f));
	}

	buf[25] = present_mask;
	buf[26] = 0x00;
	buf[27] = 0x00;
	buf[28] = 0x00;

	LOG_DBG("Payload encoded successfully (%u bytes)", PAYLOAD_SIZE);
	return 0;
}

size_t payload_get_size(void)
{
	return PAYLOAD_SIZE;
}
