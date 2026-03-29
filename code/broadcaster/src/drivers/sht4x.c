/**
 * sht4x.c
 *
 * SHT4X driver wrapper using Zephyr sensor API.
 *
 * Author: Nils Lahaye (2026)
 */

#include "sht4x.h"

LOG_MODULE_REGISTER(SHT4X, CONFIG_SHT4X_LOG_LEVEL);

const struct device *const sht4x = DEVICE_DT_GET_ONE(sensirion_sht4x);

static bool isInitialized = false;

int sht4x_init()
{
    if (isInitialized)
    {
        LOG_WRN("device already initialized");
        return 0;
    }

    LOG_INF("init");

    RET_IF_ERR(!device_is_ready(sht4x), "I2C device not ready");

    LOG_INF("Init done");

    isInitialized = true;

    return 0;
}

int sht4x_read(float *temperature, float *humidity)
{
    LOG_INF("Reading sensor");

    if (!isInitialized)
    {
        LOG_ERR("Not initialized");
        return 1;
    }

    struct sensor_value temp, hum;

    LOG_IF_ERR(sensor_sample_fetch(sht4x), "Sample fetch failed");

    LOG_IF_ERR(sensor_channel_get(sht4x, SENSOR_CHAN_AMBIENT_TEMP, &temp), "Temperature read failed");
    LOG_IF_ERR(sensor_channel_get(sht4x, SENSOR_CHAN_HUMIDITY, &hum), "Humidity read failed");

    *temperature = (float)sensor_value_to_double(&temp);
    *humidity = (float)sensor_value_to_double(&hum);

    LOG_DBG("Temperature: %f", (double)*temperature);
    LOG_DBG("Humidity: %f", (double)*humidity);

    LOG_INF("Read done");

    return 0;
}
