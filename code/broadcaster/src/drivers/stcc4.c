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
static uint16_t sensor_status;
static int64_t last_sample_ts_ms;
static int64_t last_co2_measurement_ms = 0;
static bool is_in_sleep = false;
static int64_t last_measurement_ms = 0;
static bool needs_conditioning = false;
static bool testing_mode_warned = false;

/* CO2 measurements must be spaced at least 5s apart per datasheet Section 3.4.6 */
#define STCC4_CO2_MIN_INTERVAL_MS 5000
/* Temperature/humidity can be read more frequently when cached */
#define STCC4_TEMP_HUMI_CACHE_WINDOW_MS 250
#define STCC4_SELF_TEST_RX_BUF_LEN 3
#define STCC4_SENSOR_STATUS_TESTING_MODE_MASK 0x0040U
#define STCC4_SELF_TEST_SUPPLY_VOLTAGE_BIT 0x0001U
#define STCC4_SELF_TEST_DEBUG_BITS_MASK 0x000EU
#define STCC4_SELF_TEST_SHT_DISCONNECTED_BIT 0x0010U
#define STCC4_SELF_TEST_MEMORY_BITS_MASK 0x0060U
#define STCC4_SAMPLE_WAIT_MAX_MS 10000
#define STCC4_SAMPLE_WAIT_RETRY_MS 250
#define STCC4_CONDITIONING_SETTLE_MS 2000

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

    if (ret)
    {
        LOG_ERR("Command 0x%04x write failed (%d)", stcc4_cmds[cmd].code, ret);
        return ret;
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

static bool stcc4_measurement_not_ready_err(int err)
{
    return (err == -EIO) || (err == -ENXIO) || (err == -EBUSY)
#ifdef EREMOTEIO
           || (err == -EREMOTEIO)
#endif
        ;
}

static void stcc4_update_conditioning_requirement(void)
{
    if ((k_uptime_get() - last_measurement_ms) > STCC4_IDLE_TIMEOUT_MS)
    {
        if (!needs_conditioning)
        {
            LOG_INF("Device idle >3 hours, conditioning needed on next measurement");
        }
        needs_conditioning = true;
    }
}

static int stcc4_fetch_sample(void)
{
    uint8_t rx_buf[STCC4_RX_BUF_LEN];
    int64_t waited_ms = 0;
    int ret;

    if (stcc4_mode == STCC4_MODE_SINGLE_SHOT)
    {
        RET_IF_ERR(stcc4_write_command(STCC4_CMD_MEASURE_SINGLE_SHOT), "Failed to trigger single-shot measurement");
    }

    RET_IF_ERR(stcc4_write_command(STCC4_CMD_READ_MEASUREMENT_RAW), "Failed to write read_measurement_raw command");

    do
    {
        ret = stcc4_read_reg(rx_buf, sizeof(rx_buf));
        if (ret == 0)
        {
            break;
        }

        if (stcc4_measurement_not_ready_err(ret) && (waited_ms < STCC4_SAMPLE_WAIT_MAX_MS))
        {
            LOG_DBG("Measurement not ready yet, retrying in %d ms", STCC4_SAMPLE_WAIT_RETRY_MS);
            k_msleep(STCC4_SAMPLE_WAIT_RETRY_MS);
            waited_ms += STCC4_SAMPLE_WAIT_RETRY_MS;
            continue;
        }

        LOG_IF_ERR(ret, "Failed to read sample data");
        return ret;
    }
    while (true);

    if (waited_ms > 0)
    {
        LOG_DBG("Measurement became ready after %lld ms", waited_ms);
    }

    co2_sample = (int16_t)sys_get_be16(rx_buf);
    temp_sample = sys_get_be16(&rx_buf[3]);
    humi_sample = sys_get_be16(&rx_buf[6]);
    sensor_status = sys_get_be16(&rx_buf[9]);

    if ((sensor_status & STCC4_SENSOR_STATUS_TESTING_MODE_MASK) != 0U)
    {
        if (!testing_mode_warned)
        {
            LOG_WRN("Sensor is in testing mode (status=0x%04x), ASC updates are paused", sensor_status);
            testing_mode_warned = true;
        }
    }
    else
    {
        testing_mode_warned = false;
    }

    has_sample = true;
    last_sample_ts_ms = k_uptime_get();
    last_co2_measurement_ms = last_sample_ts_ms;

    return 0;
}

static int stcc4_run_self_test(void)
{
    uint8_t rx_buf[STCC4_SELF_TEST_RX_BUF_LEN];
    uint16_t self_test_result;

    LOG_INF("Running STCC4 self-test");

    RET_IF_ERR(stcc4_write_command(STCC4_CMD_SELF_TEST), "Failed to send self-test command");
    RET_IF_ERR(stcc4_read_reg(rx_buf, sizeof(rx_buf)), "Failed to read self-test result");

    self_test_result = sys_get_be16(rx_buf);

    if ((self_test_result == 0x0000U) || (self_test_result == 0x0010U))
    {
        LOG_INF("STCC4 self-test passed (0x%04x)", self_test_result);
        return 0;
    }

    LOG_WRN("STCC4 self-test reported flags: 0x%04x", self_test_result);

    if ((self_test_result & STCC4_SELF_TEST_SUPPLY_VOLTAGE_BIT) != 0U)
    {
        LOG_WRN("Self-test: supply voltage out of specified range");
    }

    if ((self_test_result & STCC4_SELF_TEST_DEBUG_BITS_MASK) != 0U)
    {
        LOG_WRN("Self-test: debug bits set (mask 0x%04x)", self_test_result & STCC4_SELF_TEST_DEBUG_BITS_MASK);
    }

    if ((self_test_result & STCC4_SELF_TEST_SHT_DISCONNECTED_BIT) != 0U)
    {
        LOG_WRN("Self-test: SHT not connected through STCC4 controller interface");
    }

    if ((self_test_result & STCC4_SELF_TEST_MEMORY_BITS_MASK) != 0U)
    {
        LOG_ERR("Self-test: memory error bits set (0x%04x)", self_test_result & STCC4_SELF_TEST_MEMORY_BITS_MASK);
    }

    return 0;
}

static int stcc4_wake_if_needed(void)
{
    int ret;

    stcc4_update_conditioning_requirement();

    if (!is_in_sleep)
    {
        return 0;
    }

    LOG_DBG("Waking device from sleep");
    ret = stcc4_write_command(STCC4_CMD_EXIT_SLEEP);
    if (ret)
    {
        LOG_WRN("Wake command failed (%d), retrying once", ret);
        k_msleep(STCC4_SAMPLE_WAIT_RETRY_MS);
        RET_IF_ERR(stcc4_write_command(STCC4_CMD_EXIT_SLEEP), "Failed to exit sleep mode on retry");
    }

    is_in_sleep = false;
    LOG_DBG("Device awake");

    return 0;
}

static int stcc4_sleep_if_idle(void)
{
    if (is_in_sleep)
    {
        return 0;
    }

    LOG_DBG("Entering sleep mode");
    RET_IF_ERR(stcc4_write_command(STCC4_CMD_ENTER_SLEEP), "Failed to enter sleep mode");

    is_in_sleep = true;
    LOG_DBG("Device in sleep mode");

    return 0;
}

static int stcc4_perform_periodic_conditioning(void)
{
    if (!needs_conditioning)
    {
        return 0;
    }

    LOG_INF("Performing periodic conditioning");

    RET_IF_ERR( stcc4_write_command(STCC4_CMD_CONDITIONING), "Failed to send conditioning command");

    /* Datasheet recommends 2s settling after conditioning before next measurement command. */
    k_msleep(STCC4_CONDITIONING_SETTLE_MS);

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

    LOG_IF_ERR(stcc4_run_self_test(), "STCC4 self-test failed");

    LOG_INF("Performing initial conditioning (22 seconds, please wait)");
    RET_IF_ERR(stcc4_write_command(STCC4_CMD_CONDITIONING), "Failed to send conditioning command");
    LOG_INF("Initial conditioning completed");

    has_sample = false;
    last_sample_ts_ms = 0;
    last_measurement_ms = k_uptime_get();
    needs_conditioning = false;
    is_in_sleep = false;

    RET_IF_ERR(stcc4_sleep_if_idle(), "Failed to enter sleep mode after init");

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

    RET_IF_ERR(stcc4_wake_if_needed(), "Failed to wake device before read");

    RET_IF_ERR(stcc4_perform_periodic_conditioning(), "Failed to perform periodic conditioning");

    RET_IF_ERR(stcc4_fetch_sample(), "Failed to fetch sample");

    last_measurement_ms = k_uptime_get();

    tmp = (int64_t)temp_sample * STCC4_MAX_TEMP;
    *temperature = (float)((tmp / 65535LL) + STCC4_MIN_TEMP) + (float)((tmp % 65535LL) / 65535.0);

    tmp = (int64_t)humi_sample * STCC4_MAX_HUMI;
    *humidity = (float)((tmp / 65535LL) + STCC4_MIN_HUMI) + (float)((tmp % 65535LL) / 65535.0);

    LOG_DBG("Temperature: %f", (double)*temperature);
    LOG_DBG("Humidity: %f", (double)*humidity);

    RET_IF_ERR(stcc4_sleep_if_idle(), "Failed to enter sleep mode after read");

    LOG_INF("Read done");

    return 0;
}

int stcc4_read_co2(float *co2)
{
    int64_t time_since_last_co2_ms;

    LOG_INF("Reading co2");

    if (!isInitialized)
    {
        LOG_ERR("Not initialized");
        return 1;
    }

    /* Check if enough time has elapsed since last CO2 measurement (5s minimum per datasheet) */
    time_since_last_co2_ms = k_uptime_get() - last_co2_measurement_ms;
    if (time_since_last_co2_ms < STCC4_CO2_MIN_INTERVAL_MS)
    {
        if (has_sample)
        {
                LOG_DBG("Reusing cached CO2 sample (%.1f s until next measurement allowed)",
                    (double)(STCC4_CO2_MIN_INTERVAL_MS - time_since_last_co2_ms) / 1000.0);
            *co2 = (float)co2_sample;
            LOG_DBG("CO2: %f", (double)*co2);
            return 0;
        }
        else
        {
            LOG_WRN("No cached CO2 sample available; measurement interval too fast");
            return -EAGAIN;
        }
    }

    /* Wake device if needed and check for periodic conditioning requirement */
    RET_IF_ERR(stcc4_wake_if_needed(), "Failed to wake device before read");

    /* Perform periodic conditioning if device idle >3 hours */
    RET_IF_ERR(stcc4_perform_periodic_conditioning(), "Failed to perform periodic conditioning");

    /* Perform single-shot measurement per datasheet Section 3.4.6 sequence */
    RET_IF_ERR(stcc4_fetch_sample(), "Failed to fetch sample");

    /* Update measurement timestamp for sampling interval enforcement */
    last_measurement_ms = k_uptime_get();

    /* Sleep after measurement; next CO2 read must wait minimum 5s per datasheet */
    RET_IF_ERR(stcc4_sleep_if_idle(), "Failed to enter sleep mode after read");

    *co2 = (float)co2_sample;

    LOG_DBG("CO2: %f ppm, Sensor Status: 0x%04x", (double)*co2, sensor_status);

    return 0;
}

int stcc4_read_all(float *temperature, float *humidity, float *co2)
{
    int64_t tmp;
    bool need_new_sample = false;
    int64_t time_since_last_co2_ms;

    LOG_INF("Reading all sensor data");

    if (!isInitialized)
    {
        LOG_ERR("Not initialized");
        return 1;
    }

    /* Check if enough time has elapsed since last CO2 measurement (5s minimum per datasheet) */
    time_since_last_co2_ms = k_uptime_get() - last_co2_measurement_ms;
    if (time_since_last_co2_ms < STCC4_CO2_MIN_INTERVAL_MS)
    {
        if (has_sample)
        {
                LOG_DBG("Reusing cached sample (%.1f s until next measurement allowed)",
                    (double)(STCC4_CO2_MIN_INTERVAL_MS - time_since_last_co2_ms) / 1000.0);
            /* Convert and return cached values */
            tmp = (int64_t)temp_sample * STCC4_MAX_TEMP;
            *temperature = (float)((tmp / 65535LL) + STCC4_MIN_TEMP) + (float)((tmp % 65535LL) / 65535.0);

            tmp = (int64_t)humi_sample * STCC4_MAX_HUMI;
            *humidity = (float)((tmp / 65535LL) + STCC4_MIN_HUMI) + (float)((tmp % 65535LL) / 65535.0);

            *co2 = (float)co2_sample;

            LOG_DBG("Cached: Temperature: %f, Humidity: %f, CO2: %f, Status: 0x%04x",
                    (double)*temperature, (double)*humidity, (double)*co2, sensor_status);
            return 0;
        }
        else
        {
            LOG_WRN("No cached sample available; measurement interval too fast");
            return -EAGAIN;
        }
    }

    /* Measurement interval satisfied; fetch fresh sample */
    need_new_sample = true;

    if (need_new_sample)
    {
        /* Wake device if needed and check for periodic conditioning requirement */
        RET_IF_ERR(stcc4_wake_if_needed(), "Failed to wake device before read");

        /* Perform periodic conditioning if device idle >3 hours */
        RET_IF_ERR(stcc4_perform_periodic_conditioning(), "Failed to perform periodic conditioning");

        /* Perform single-shot measurement per datasheet Section 3.4.6 sequence */
        RET_IF_ERR(stcc4_fetch_sample(), "Failed to fetch sample");

        /* Update measurement timestamp for sampling interval enforcement */
        last_measurement_ms = k_uptime_get();

        /* Sleep after measurement; next read must wait minimum 5s per datasheet */
        RET_IF_ERR(stcc4_sleep_if_idle(), "Failed to enter sleep mode after read");
    }

    /* Convert raw values to float */
    tmp = (int64_t)temp_sample * STCC4_MAX_TEMP;
    *temperature = (float)((tmp / 65535LL) + STCC4_MIN_TEMP) + (float)((tmp % 65535LL) / 65535.0);

    tmp = (int64_t)humi_sample * STCC4_MAX_HUMI;
    *humidity = (float)((tmp / 65535LL) + STCC4_MIN_HUMI) + (float)((tmp % 65535LL) / 65535.0);

    *co2 = (float)co2_sample;

    LOG_DBG("Temperature: %f, Humidity: %f, CO2: %f ppm, Sensor Status: 0x%04x",
            (double)*temperature, (double)*humidity, (double)*co2, sensor_status);
    LOG_INF("Read all done");

    return 0;
}
