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
static bool has_sample = false;
static int16_t co2_sample;
static uint16_t temp_sample;
static uint16_t humi_sample;
static int64_t last_sample_ts_ms;
static bool is_in_sleep = false;
static int64_t last_measurement_ms = 0;
static bool needs_conditioning = false;

#define STCC4_SAMPLE_CACHE_WINDOW_MS 250
#define STCC4_SAMPLE_WAIT_MAX_MS 10000
#define STCC4_SAMPLE_WAIT_RETRY_MS 250

static const struct stcc4_cmd stcc4_cmds[] = {
    [STCC4_CMD_START_CONTINUOUS] = {0x218BU, 0U},
    [STCC4_CMD_READ_MEASUREMENT_RAW] = {0xEC05U, 1U},
    [STCC4_CMD_STOP_CONTINUOUS] = {0x3F86U, 1200U},
    [STCC4_CMD_MEASURE_SINGLE_SHOT] = {0x219DU, 500U},
    [STCC4_CMD_ENTER_SLEEP] = {0x3650U, 1U},
    [STCC4_CMD_EXIT_SLEEP] = {0x0000U, 5U},
    [STCC4_CMD_SELF_TEST] = {0x278CU, 360U},
    [STCC4_CMD_GET_PRODUCT_ID] = {0x365BU, 1U},
    [STCC4_CMD_CONDITIONING] = {0x29BCU, STCC4_CONDITIONING_DURATION_MS},
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

    RET_IF_ERR(i2c_read_dt(&stcc4_spec, rx_buf, rx_buf_size), "Failed to read register");

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
        RET_IF_ERR(stcc4_write_command(STCC4_CMD_MEASURE_SINGLE_SHOT), "Failed to trigger single-shot measurement");
    }

    RET_IF_ERR(stcc4_write_command(STCC4_CMD_READ_MEASUREMENT_RAW), "Failed to write read_measurement_raw command");

    ret = stcc4_read_reg(rx_buf, sizeof(rx_buf));
    if (ret < 0)
    {
        if(stcc4_mode != STCC4_MODE_CONTINUOUS)
        {
            LOG_IF_ERR(ret, "Failed to read sample data");
            return ret;
        }
        
        if ((ret == -EIO) || (ret == -ENXIO) || (ret == -EBUSY)
#ifdef EREMOTEIO
                || (ret == -EREMOTEIO)
#endif
            )
            {
                LOG_DBG("Measurement not ready yet");
                return -EAGAIN;
            }

            RET_IF_ERR(ret, "Failed to read sample data in continuous mode");
    }

    co2_sample = (int16_t)sys_get_be16(rx_buf);
    temp_sample = sys_get_be16(&rx_buf[3]);
    humi_sample = sys_get_be16(&rx_buf[6]);
    has_sample = true;
    last_sample_ts_ms = k_uptime_get();

    return 0;
}

static int stcc4_wait_for_sample(int64_t max_wait_ms)
{
    int ret;
    int64_t deadline_ms = k_uptime_get() + max_wait_ms;
    uint16_t retries = 0U;

    do
    {
        ret = stcc4_fetch_sample();
        if (ret == 0)
        {
            return 0;
        }

        if (ret != -EAGAIN)
        {
            return ret;
        }

        retries++;
        if ((retries % 4U) == 0U)
        {
            LOG_DBG("Measurement not ready yet (retry %u)", retries);
        }

        k_msleep(STCC4_SAMPLE_WAIT_RETRY_MS);
    } while (k_uptime_get() < deadline_ms);

    LOG_ERR("Timeout waiting for valid STCC4 sample (%lld ms)", max_wait_ms);
    return -ETIMEDOUT;
}

static void stcc4_wake_if_needed(void)
{
    if (!is_in_sleep)
    {
        if ((k_uptime_get() - last_measurement_ms) <= STCC4_IDLE_TIMEOUT_MS)
        {
            return;
        }
        LOG_INF("Device idle >3 hours, conditioning needed on next measurement");
        needs_conditioning = true;
        return;
    }

    LOG_DBG("Waking device from sleep");
    RET_IF_ERR(stcc4_write_command(STCC4_CMD_EXIT_SLEEP), "Failed to exit sleep mode");

    is_in_sleep = false;
    LOG_DBG("Device awake");
}

static void stcc4_sleep_if_idle(void)
{
    if (is_in_sleep)
    {
        return;
    }

    LOG_DBG("Entering sleep mode");
    RET_IF_ERR(stcc4_write_command(STCC4_CMD_ENTER_SLEEP), "Failed to enter sleep mode");

    is_in_sleep = true;
    LOG_DBG("Device in sleep mode");
}

static int stcc4_perform_periodic_conditioning(void)
{
    if (!needs_conditioning)
    {
        return 0;
    }

    LOG_INF("Performing periodic conditioning");

    RET_IF_ERR( stcc4_write_command(STCC4_CMD_CONDITIONING), "Failed to send conditioning command");

    last_measurement_ms = k_uptime_get();
    needs_conditioning = false;
    LOG_INF("Conditioning completed");

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

    LOG_IF_ERR(stcc4_write_command(STCC4_CMD_STOP_CONTINUOUS),
               "Failed to stop continuous measurement (may not have been running)");

    RET_IF_ERR(stcc4_write_command(STCC4_CMD_GET_PRODUCT_ID), "Failed to send get_product_id command");
    RET_IF_ERR(stcc4_read_reg(rx_buf, sizeof(rx_buf)), "Failed to read product ID");

    id_hi = sys_get_be16(rx_buf);
    id_lo = sys_get_be16(&rx_buf[3]);
    LOG_INF("Product ID: 0x%04x%04x", id_hi, id_lo);

    LOG_INF("Performing initial conditioning (22 seconds, please wait)");
    RET_IF_ERR(stcc4_write_command(STCC4_CMD_CONDITIONING), "Failed to send conditioning command");
    LOG_INF("Initial conditioning completed");

    has_sample = false;
    last_sample_ts_ms = 0;
    last_measurement_ms = k_uptime_get();
    needs_conditioning = false;
    is_in_sleep = false;

    stcc4_sleep_if_idle();

    isInitialized = true;
    LOG_INF("Init done");

    return 0;
}

int stcc4_read(float *temperature, float *humidity)
{
    int64_t tmp;

    LOG_INF("Reading sensor");

    if (!isInitialized)
    {
        LOG_ERR("Not initialized");
        return 1;
    }

    stcc4_wake_if_needed();

    RET_IF_ERR(stcc4_perform_periodic_conditioning(), "Failed to perform periodic conditioning");

    RET_IF_ERR(stcc4_fetch_sample(), "Failed to fetch sample");

    last_measurement_ms = k_uptime_get();

    tmp = (int64_t)temp_sample * STCC4_MAX_TEMP;
    *temperature = (float)((tmp / 65535LL) + STCC4_MIN_TEMP) + (float)((tmp % 65535LL) / 65535.0);

    tmp = (int64_t)humi_sample * STCC4_MAX_HUMI;
    *humidity = (float)((tmp / 65535LL) + STCC4_MIN_HUMI) + (float)((tmp % 65535LL) / 65535.0);

    LOG_DBG("Temperature: %f", (double)*temperature);
    LOG_DBG("Humidity: %f", (double)*humidity);

    stcc4_sleep_if_idle();

    LOG_INF("Read done");

    return 0;
}

int stcc4_read_co2(float *co2)
{
    bool need_new_sample = false;
    int64_t cache_age_ms;

    LOG_INF("Reading co2");

    if (!isInitialized)
    {
        LOG_ERR("Not initialized");
        return 1;
    }

    /* Check if we need a fresh sample (cache expired or empty) */
    if (!has_sample)
    {
        need_new_sample = true;
    }
    else
    {
        cache_age_ms = k_uptime_get() - last_sample_ts_ms;
        if (cache_age_ms > STCC4_SAMPLE_CACHE_WINDOW_MS)
        {
            need_new_sample = true;
        }
    }

    if (need_new_sample)
    {
        /* Wake device if needed and check for periodic conditioning requirement */
        stcc4_wake_if_needed();

        /* Perform periodic conditioning if device idle >3 hours */
        RET_IF_ERR(stcc4_perform_periodic_conditioning(), "Failed to perform periodic conditioning");

        /* Perform single-shot measurement */
        RET_IF_ERR(stcc4_fetch_sample(), "Failed to fetch sample");

        /* Update measurement timestamp */
        last_measurement_ms = k_uptime_get();

        /* Sleep after measurement (cache allows reuse within 250ms window) */
        stcc4_sleep_if_idle();
    }

    *co2 = (float)co2_sample;

    LOG_DBG("CO2: %f", (double)*co2);

    return 0;
}

int stcc4_read_all(float *temperature, float *humidity, float *co2)
{
    int64_t tmp;
    bool need_new_sample = false;
    int64_t cache_age_ms;

    LOG_INF("Reading all sensor data");

    if (!isInitialized)
    {
        LOG_ERR("Not initialized");
        return 1;
    }

    /* Check if we need a fresh sample (same logic as stcc4_read_co2) */
    if (!has_sample)
    {
        need_new_sample = true;
    }
    else
    {
        cache_age_ms = k_uptime_get() - last_sample_ts_ms;
        if (cache_age_ms > STCC4_SAMPLE_CACHE_WINDOW_MS)
        {
            need_new_sample = true;
        }
    }

    if (need_new_sample)
    {
        /* Single wake for all three reads (batched efficiency) */
        stcc4_wake_if_needed();

        /* Perform periodic conditioning if device idle >3 hours */
        RET_IF_ERR(stcc4_perform_periodic_conditioning(), "Failed to perform periodic conditioning");

        /* Perform single-shot measurement */
        ret = stcc4_fetch_sample();
        RET_IF_ERR(ret, "Failed to fetch sample");

        /* Update measurement timestamp */
        last_measurement_ms = k_uptime_get();

        /* Sleep after all measurements (cache allows reuse within 250ms window) */
        stcc4_sleep_if_idle();
    }

    /* Convert raw values to float */
    tmp = (int64_t)temp_sample * STCC4_MAX_TEMP;
    *temperature = (float)((tmp / 65535LL) + STCC4_MIN_TEMP) + (float)((tmp % 65535LL) / 65535.0);

    tmp = (int64_t)humi_sample * STCC4_MAX_HUMI;
    *humidity = (float)((tmp / 65535LL) + STCC4_MIN_HUMI) + (float)((tmp % 65535LL) / 65535.0);

    *co2 = (float)co2_sample;

    LOG_DBG("Temperature: %f, Humidity: %f, CO2: %f", (double)*temperature, (double)*humidity, (double)*co2);
    LOG_INF("Read all done");

    return 0;
}
