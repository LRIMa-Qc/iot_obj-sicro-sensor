/**
 * stcc4.h
 *
 * STCC4 driver wrapper based on upstream Zephyr STCC4 implementation.
 *
 * Author: Nils Lahaye (2026)
 */

#ifndef STCC4_H_
#define STCC4_H_

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include "../utils.h"

#define STCC4_CRC_POLY 0x31U
#define STCC4_CRC_INIT 0xFFU

#define STCC4_WORD_SIZE 3U
#define STCC4_MEASUREMENT_WORDS 4U
#define STCC4_RX_BUF_LEN (STCC4_MEASUREMENT_WORDS * STCC4_WORD_SIZE)

#define STCC4_WAKEUP_BYTE 0x00U

#define STCC4_CMD_START_CONTINUOUS 0U
#define STCC4_CMD_READ_MEASUREMENT_RAW 1U
#define STCC4_CMD_STOP_CONTINUOUS 2U
#define STCC4_CMD_MEASURE_SINGLE_SHOT 3U
#define STCC4_CMD_ENTER_SLEEP 4U
#define STCC4_CMD_EXIT_SLEEP 5U
#define STCC4_CMD_SELF_TEST 6U
#define STCC4_CMD_GET_PRODUCT_ID 7U
#define STCC4_CMD_CONDITIONING 8U

#define STCC4_IDLE_TIMEOUT_MS (3U * 60U * 60U * 1000U) /* 3 hours */
#define STCC4_CONDITIONING_DURATION_MS 22000U          /* 22 seconds */

#define STCC4_MAX_TEMP 175
#define STCC4_MIN_TEMP (-45)
#define STCC4_MAX_HUMI 125
#define STCC4_MIN_HUMI (-6)

enum stcc4_mode
{
    STCC4_MODE_CONTINUOUS = 0,
    STCC4_MODE_SINGLE_SHOT = 1,
};

struct stcc4_cmd
{
    uint16_t code;
    uint16_t duration_ms;
};

int stcc4_init(void);

int stcc4_read(float *temperature, float *humidity);

int stcc4_read_co2(float *co2);

int stcc4_read_all(float *temperature, float *humidity, float *co2);

#endif /* STCC4_H_ */
