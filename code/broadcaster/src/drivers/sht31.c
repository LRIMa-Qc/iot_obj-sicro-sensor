/**
 * sht31.c
 * 
 * This file is used to define the SHT31 sensor constants and functions.
 * 
 * Author: Nils Lahaye (2024)
 * 
*/

#include "sht31.h"

LOG_MODULE_REGISTER(SHT31, CONFIG_SHT31_LOG_LEVEL); /* Register the module for log */

const struct device *const sht31 = DEVICE_DT_GET_ONE(sensirion_sht3xd);

static bool isInitialized = false; /* Is the sensor initialized? */

/**
 * @brief Initalise the SHT31 sensor
 * 
 * @return 0 on success, error code otherwise
*/
int sht31_init()
{
    if(isInitialized) {
        LOG_WRN("device already initialized");
        return 0;
    }

    LOG_INF("init");

    RET_IF_ERR(!device_is_ready(sht31), "I2C device not ready");

    LOG_INF("Init done");

    isInitialized = true;

    return 0;
}

/**
 * @brief Read the temperature and humidity from the SHT31 sensor
 * 
 * @param temperature pointer to the variable where the temperature will be stored
 * @param humidity pointer to the variable where the humidity will be stored
 * 
 * @return 0 on success, error code otherwise
*/
int sht31_read(float *temperature, float *humidity)
{
    LOG_INF("Reading sensor");

    if (!isInitialized)
    {
        LOG_ERR("Not initialized");
        return 1;
    }

    struct sensor_value temp, hum;

    LOG_IF_ERR(sensor_sample_fetch(sht31), "Sample fetch failed");

    LOG_IF_ERR(sensor_channel_get(sht31, SENSOR_CHAN_AMBIENT_TEMP, &temp), "Temperature read failed");
    LOG_IF_ERR(sensor_channel_get(sht31, SENSOR_CHAN_HUMIDITY, &hum), "Humidity read failed");

    *temperature = (float) sensor_value_to_double(&temp);
    *humidity = (float) sensor_value_to_double(&hum);

    LOG_DBG("Temperature: %f", *temperature);
    LOG_DBG("Humidity: %f", *humidity);

    LOG_INF("Read done");

    return 0;
}