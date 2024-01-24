/**
 * sth20.c
 *
 * This file is used to define the SHT20 sensor constants and functions.
 *
 * Author: Francis Mathieu-Gosselin 2024
 *
 */

#include "sht20.h"

LOG_MODULE_REGISTER(SHT20, CONFIG_SHT20_LOG_LEVEL); /* Register the module for log */

static const struct i2c_dt_spec sht20_spec = I2C_DT_SPEC_GET(DT_NODELABEL(sht20)); /* AHT20 i2c spec */

static bool isInitialized = false; /* Is the sensor initialized? */

static uint8_t cmdBuff[4];       /* Command buffer */
static uint8_t dataBuff[4];      /* Data buffer */
static uint32_t humidity_raw;    /* Humidity raw value */
static uint32_t temperature_raw; /* Temperature raw value */

/**
 * @brief Initalise the AHT20 sensor on i2c bus 1
 *
 * @return 0 on success, error code otherwise
 */
int sht20_init()
{
    if (isInitialized)
    {
        LOG_WRN("device already initialized");
        return 0;
    }

    LOG_INF("init");

    RET_IF_ERR(!device_is_ready(sht20_spec.bus), "I2C device not ready");

    cmdBuff[0] = SHT20_SOFT_RESET;
    RET_IF_ERR(i2c_write_dt(&sht20_spec, cmdBuff, 1), "reset failed");

    k_sleep(K_MSEC(15)); /* Wait for the sensor to reset */

    LOG_INF("Init done");

    isInitialized = true;

    return 0;
}

/**
 * @brief Read the temperature and humidity from the AHT20 sensor
 *
 * @param temperature pointer to the variable where the temperature will be stored
 * @param humidity pointer to the variable where the humidity will be stored
 *
 * @return 0 on success, error code otherwise
 */
int sht20_read(float *temperature, float *humidity)
{
    LOG_INF("Reading sensor");

    if (!isInitialized)
    {
        LOG_ERR("Not initialized");
        return 1;
    }

    cmdBuff[0] = SHT20_TRIGGER_T_MEASURE_HOLD;
    // cmdBuff[1] = AHT20_TRIGGER_MEASURE_BYTE_0;
    // cmdBuff[2] = AHT20_TRIGGER_MEASURE_BYTE_1;
    LOG_IF_ERR(i2c_write_dt(&sht20_spec, cmdBuff, 1), "trigger measure failed");

    k_sleep(K_MSEC(60));
    while (1)
    {
        LOG_IF_ERR(i2c_read_dt(&sht20_spec, dataBuff, 3), "read failed");
        /* Check if the data is ready */
        if (dataBuff[3])
            k_sleep(K_MSEC(5));
        else
            break;
    }

    temperature_raw = dataBuff[0];
    temperature_raw <<= 8;
    temperature_raw |= dataBuff[1];

    *temperature = ((float)temperature_raw * 0.00268127) - 46.85;

    // humidity
    cmdBuff[0] = SHT20_TRIGGER_RH_MEASURE_HOLD;
    LOG_IF_ERR(i2c_write_dt(&sht20_spec, cmdBuff, 1), "trigger measure failed");

    k_sleep(K_MSEC(60));
    while (1)
    {
        LOG_IF_ERR(i2c_read_dt(&sht20_spec, dataBuff, 3), "read failed");
        /* Check if the data is ready */
        if (dataBuff[3])
            k_sleep(K_MSEC(5));
        else
            break;
    }

    humidity_raw = dataBuff[0];
    humidity_raw <<= 8;
    humidity_raw |= dataBuff[1];

    *humidity = ((float)humidity_raw * 0.00190734863) - 6;

    LOG_DBG("Raw data: %02x %02x %02x",
            dataBuff[0], dataBuff[1], dataBuff[2]);
    LOG_DBG("Temperature raw: %d \t converted : %d.%dC", temperature_raw, (int)*temperature, (int)(*temperature * 10) % 10);
    LOG_DBG("Humidity raw: %d \t converted : %d.%d%%", humidity_raw, (int)*humidity, (int)(*humidity * 10) % 10);

    LOG_INF("Read done");

    return 0;
}