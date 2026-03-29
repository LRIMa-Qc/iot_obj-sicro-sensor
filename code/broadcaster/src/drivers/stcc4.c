/**
 * stcc4.c
 *
 * STCC4 driver wrapper based on upstream Zephyr STCC4 implementation.
 *
 * Author: Nils Lahaye (2026)
 */

#include "stcc4.h"

LOG_MODULE_REGISTER(STCC4, CONFIG_STCC4_LOG_LEVEL);

static const struct i2c_dt_spec stcc4_spec = I2C_DT_SPEC_GET(DT_NODELABEL(stcc4));
static const enum stcc4_mode stcc4_mode = DT_PROP(DT_NODELABEL(stcc4), measurement_mode);

static bool isInitialized = false;
static int16_t co2_sample;
static uint16_t temp_sample;
static uint16_t humi_sample;

static const struct stcc4_cmd stcc4_cmds[] = {
    [STCC4_CMD_START_CONTINUOUS] = {0x218BU, 0U},
    [STCC4_CMD_READ_MEASUREMENT_RAW] = {0xEC05U, 1U},
    [STCC4_CMD_STOP_CONTINUOUS] = {0x3F86U, 1200U},
    [STCC4_CMD_MEASURE_SINGLE_SHOT] = {0x219DU, 500U},
    [STCC4_CMD_ENTER_SLEEP] = {0x3650U, 1U},
    [STCC4_CMD_EXIT_SLEEP] = {0x0000U, 5U},
    [STCC4_CMD_SELF_TEST] = {0x278CU, 360U},
    [STCC4_CMD_GET_PRODUCT_ID] = {0x365BU, 1U},
};

static uint8_t stcc4_compute_crc(uint16_t value)
{
    uint8_t buf[2];

    sys_put_be16(value, buf);

    return crc8(buf, 2, STCC4_CRC_POLY, STCC4_CRC_INIT, false);
}

static int stcc4_write_command(uint8_t cmd)
{
    int ret;

    if (cmd == STCC4_CMD_EXIT_SLEEP)
    {
        uint8_t tx_buf[1] = {STCC4_WAKEUP_BYTE};
        ret = i2c_write_dt(&stcc4_spec, tx_buf, sizeof(tx_buf));
    }
    else
    {
        uint8_t tx_buf[2];

        sys_put_be16(stcc4_cmds[cmd].code, tx_buf);
        ret = i2c_write_dt(&stcc4_spec, tx_buf, sizeof(tx_buf));
    }

    if (stcc4_cmds[cmd].duration_ms != 0U)
    {
        k_msleep(stcc4_cmds[cmd].duration_ms);
    }

    return ret;
}

static int stcc4_read_reg(uint8_t *rx_buf, uint8_t rx_buf_size)
{
    uint8_t num_words = rx_buf_size / STCC4_WORD_SIZE;
    int ret;

    ret = i2c_read_dt(&stcc4_spec, rx_buf, rx_buf_size);
    if (ret < 0)
    {
        return ret;
    }

    for (uint8_t i = 0U; i < num_words; i++)
    {
        uint8_t offset = i * STCC4_WORD_SIZE;
        uint8_t crc = stcc4_compute_crc(sys_get_be16(&rx_buf[offset]));

        if (crc != rx_buf[offset + 2U])
        {
            LOG_ERR("Invalid CRC (expected 0x%02x, got 0x%02x)", crc, rx_buf[offset + 2U]);
            return -EIO;
        }
    }

    return 0;
}

static int stcc4_fetch_sample(void)
{
    uint8_t rx_buf[STCC4_RX_BUF_LEN];
    int ret;

    if (stcc4_mode == STCC4_MODE_SINGLE_SHOT)
    {
        ret = stcc4_write_command(STCC4_CMD_MEASURE_SINGLE_SHOT);
        if (ret < 0)
        {
            LOG_ERR("Failed to trigger single-shot measurement");
            return ret;
        }
    }

    ret = stcc4_write_command(STCC4_CMD_READ_MEASUREMENT_RAW);
    if (ret < 0)
    {
        LOG_ERR("Failed to write read_measurement_raw command");
        return ret;
    }

    ret = stcc4_read_reg(rx_buf, sizeof(rx_buf));
    if (ret < 0)
    {
        if (stcc4_mode == STCC4_MODE_CONTINUOUS)
        {
            LOG_DBG("Measurement not ready yet");
            return 0;
        }

        LOG_ERR("Failed to read sample data");
        return ret;
    }

    co2_sample = (int16_t)sys_get_be16(rx_buf);
    temp_sample = sys_get_be16(&rx_buf[3]);
    humi_sample = sys_get_be16(&rx_buf[6]);

    return 0;
}

int stcc4_init(void)
{
    uint8_t rx_buf[STCC4_RX_BUF_LEN];
    uint16_t id_hi;
    uint16_t id_lo;

    if (isInitialized)
    {
        LOG_WRN("device already initialized");
        return 0;
    }

    LOG_INF("init");

    RET_IF_ERR(!device_is_ready(stcc4_spec.bus), "I2C device not ready");

    (void)stcc4_write_command(STCC4_CMD_EXIT_SLEEP);

    if (stcc4_write_command(STCC4_CMD_STOP_CONTINUOUS) < 0)
    {
        LOG_WRN("Failed to stop continuous measurement (may not have been running)");
    }

    RET_IF_ERR(stcc4_write_command(STCC4_CMD_GET_PRODUCT_ID), "Failed to send get_product_id command");
    RET_IF_ERR(stcc4_read_reg(rx_buf, sizeof(rx_buf)), "Failed to read product ID");

    id_hi = sys_get_be16(rx_buf);
    id_lo = sys_get_be16(&rx_buf[3]);
    LOG_INF("Product ID: 0x%04x%04x", id_hi, id_lo);

    if (stcc4_mode == STCC4_MODE_CONTINUOUS)
    {
        RET_IF_ERR(stcc4_write_command(STCC4_CMD_START_CONTINUOUS), "Failed to start continuous measurement");
    }

    isInitialized = true;
    LOG_INF("Init done");

    return 0;
}

int stcc4_read(float *temperature, float *humidity)
{
    int ret;
    int64_t tmp;

    LOG_INF("Reading sensor");

    if (!isInitialized)
    {
        LOG_ERR("Not initialized");
        return 1;
    }

    ret = stcc4_fetch_sample();
    if (ret < 0)
    {
        return ret;
    }

    tmp = (int64_t)temp_sample * STCC4_MAX_TEMP;
    *temperature = (float)((tmp / 65535LL) + STCC4_MIN_TEMP) + (float)((tmp % 65535LL) / 65535.0);

    tmp = (int64_t)humi_sample * STCC4_MAX_HUMI;
    *humidity = (float)((tmp / 65535LL) + STCC4_MIN_HUMI) + (float)((tmp % 65535LL) / 65535.0);

    LOG_DBG("Temperature: %f", (double)*temperature);
    LOG_DBG("Humidity: %f", (double)*humidity);

    LOG_INF("Read done");

    return 0;
}

int stcc4_read_co2(float *co2)
{
    int ret;

    LOG_INF("Reading co2");

    if (!isInitialized)
    {
        LOG_ERR("Not initialized");
        return 1;
    }

    ret = stcc4_fetch_sample();
    if (ret < 0)
    {
        return ret;
    }

    *co2 = (float)co2_sample;

    LOG_DBG("CO2: %f", (double)*co2);

    return 0;
}
