/**
 * adc.c
 * 
 * This file will read the adc values from the sensors
 * 
 * Author: Nils Lahaye 2023
 * 
*/
#include "adc.h"

LOG_MODULE_REGISTER(ADC, CONFIG_ADC_LOG_LEVEL); /* Register the module for log */

/* Ground humidity sensor */
static const struct adc_dt_spec hum_adc_spec = ADC_DT_SPEC_GET(DT_NODELABEL(ground_humidity));
static const struct gpio_dt_spec hum_enable_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(ground_humidity), power_gpios);
static const int dry_value[3] = DT_PROP(DT_NODELABEL(ground_humidity), dry);
static const int wet_value[3] = DT_PROP(DT_NODELABEL(ground_humidity), wet);
/* Ground temperature sensor */
static const struct adc_dt_spec temp_adc_spec = ADC_DT_SPEC_GET(DT_NODELABEL(ground_temperature));
static const struct gpio_dt_spec temp_enable_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(ground_temperature), power_gpios);
static const uint16_t ground_temp_resistor = DT_PROP(DT_NODELABEL(ground_temperature), output_ohms);
/* Luminosity sensor */
static const struct adc_dt_spec pt19_adc_spec = ADC_DT_SPEC_GET(DT_NODELABEL(pt19));
static const struct gpio_dt_spec pt19_enable_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(pt19), power_gpios);
/* Battery voltage sensor */
static const struct adc_dt_spec bat_adc_spec = ADC_DT_SPEC_GET(DT_NODELABEL(battery_voltage));

/* Value for all sensors */
static int16_t sample_buffer;
static struct adc_sequence sequence = {
    .buffer		    = &sample_buffer,
    .buffer_size	= sizeof(sample_buffer),
};

static bool isInisialized = false;

/**
 * @brief Initalise the adc
 * 
 * @return int 0 if success, error code otherwise
*/
int adc_init() {
    if(isInisialized) {
        LOG_WRN("adc devices already initialized");
        return 0;
    }

    LOG_INF("init");

    /* Init ground humidity sensor */
    RET_IF_ERR(!device_is_ready(hum_adc_spec.dev), "Ground humidity ADC device not ready");
    RET_IF_ERR(adc_channel_setup_dt(&hum_adc_spec), "Ground humidity ADC channel setup failed");
    RET_IF_ERR(gpio_pin_configure_dt(&hum_enable_spec, GPIO_OUTPUT), "Ground humidity GPIO pin configuration failed");
    /* Init ground temperature sensor */
    RET_IF_ERR(!device_is_ready(temp_adc_spec.dev), "Ground temperature ADC device not ready");
    RET_IF_ERR(adc_channel_setup_dt(&temp_adc_spec), "Ground temperature ADC channel setup failed");
    RET_IF_ERR(gpio_pin_configure_dt(&temp_enable_spec, GPIO_OUTPUT), "Ground temperature GPIO pin configuration failed");
    /* Init luminosity sensor */
    RET_IF_ERR(!device_is_ready(pt19_adc_spec.dev), "Luminosity ADC device not ready");
    RET_IF_ERR(adc_channel_setup_dt(&pt19_adc_spec), "Luminosity ADC channel setup failed");
    RET_IF_ERR(gpio_pin_configure_dt(&pt19_enable_spec, GPIO_OUTPUT), "Luminosity GPIO pin configuration failed");
    /* Init battery voltage sensor */
    RET_IF_ERR(!device_is_ready(bat_adc_spec.dev), "Battery voltage ADC device not ready");
    RET_IF_ERR(adc_channel_setup_dt(&bat_adc_spec), "Battery voltage ADC channel setup failed");

    /* Deactivate power to the sensors */
    RET_IF_ERR(gpio_pin_set_dt(&hum_enable_spec, 0), "Ground humidity GPIO pin set failed");
    RET_IF_ERR(gpio_pin_set_dt(&temp_enable_spec, 0), "Ground temperature GPIO pin set failed");
    RET_IF_ERR(gpio_pin_set_dt(&pt19_enable_spec, 0), "Luminosity GPIO pin set failed");

    isInisialized = true;

    LOG_INF("init done");

    return 0;
}

/**
 * @brief Read the ground humidity (0-100)
 * 
 * @param humidity Pointer to the humidity value
 * @return int 0 if success, error code otherwise
*/
int ground_humidity_read(float *humidity) {
    LOG_INF("ground humidity read");

    if(!isInisialized) {
        LOG_ERR("adc devices not initialized");
        return -1;
    }

    /* Activate power to the sensor */
    RET_IF_ERR(gpio_pin_set_dt(&hum_enable_spec, 1), "Ground humidity GPIO pin set failed");
    k_sleep(K_MSEC(40)); /* Wait for the sensor to be ready */

    /* Read the value */
    RET_IF_ERR(adc_sequence_init_dt(&hum_adc_spec, &sequence), "Ground humidity ADC sequence init failed");
    RET_IF_ERR(adc_read(hum_adc_spec.dev, &sequence), "Ground humidity ADC read failed");

    /* Deactivate power to the sensor */
    RET_IF_ERR(gpio_pin_set_dt(&hum_enable_spec, 0), "Ground humidity GPIO pin set failed");

    int16_t raw = sample_buffer;

    float battery_voltage;
    RET_IF_ERR(battery_voltage_read(&battery_voltage), "Battery voltage read failed");

    /* Convert the value to a percentage */
    float dry = evaluate_polynomial(battery_voltage, dry_value);
    float wet = evaluate_polynomial(battery_voltage, wet_value);

    /* Convert the value to a percentage */
    *humidity = (raw - dry_value[0]) * 100 / (wet_value[0] - dry_value[0]);
    *humidity = mapRange(raw, dry, wet, 0, 100);

    LOG_DBG("Ground humidity | raw: %d \t humidity: %d.%d%%", raw, (int)*humidity, (int)(*humidity * 100) % 100);

    LOG_INF("ground humidity read done");

    return 0;
}

/**
 * @brief Read the ground temperature in C
 * 
 * @param temperature Pointer to the temperature value
 * @return int 0 if success, error code otherwise
*/
int ground_temperature_read(float *temperature) {
    LOG_INF("ground temperature read");

    if(!isInisialized) {
        LOG_ERR("adc devices not initialized");
        return -1;
    }

    /* Activate power to the sensor */
    RET_IF_ERR(gpio_pin_set_dt(&temp_enable_spec, 1), "Ground temperature GPIO pin set failed");
    k_sleep(K_MSEC(40)); /* Wait for the sensor to be ready */

    /* Read the value */
    RET_IF_ERR(adc_sequence_init_dt(&temp_adc_spec, &sequence), "Ground temperature ADC sequence init failed");
    RET_IF_ERR(adc_read(temp_adc_spec.dev, &sequence), "Ground temperature ADC read failed");

    /* Deactivate power to the sensor */
    RET_IF_ERR(gpio_pin_set_dt(&temp_enable_spec, 0), "Ground temperature GPIO pin set failed");

    /* Convert the value to a temperature */
    float resistance = (float)ground_temp_resistor * (1023.0 / (float)sample_buffer - 1.0);
	*temperature = log(resistance);
	*temperature = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * *temperature * *temperature ))* *temperature );
	*temperature = *temperature - 273.15; // Convert Kelvin to Celcius

    LOG_DBG("Ground temperature | raw: %d \t resistance: %d.%d \t temperature: %d.%d°C", sample_buffer, (int)resistance, (int)(resistance * 100) % 100, (int)*temperature, (int)(*temperature * 100) % 100);

    LOG_INF("ground temperature read done");

    return 0;
}

/**
 * @brief Read the luminosity (0-100)
 * 
 * @param luminosity Pointer to the luminosity value
 * @return int 0 if success, error code otherwise
*/
int luminosity_read(float *luminosity) {
    LOG_INF("luminosity read");

    if(!isInisialized) {
        LOG_ERR("adc devices not initialized");
        return -1;
    }

    /* Activate power to the sensor */
    RET_IF_ERR(gpio_pin_set_dt(&pt19_enable_spec, 1), "Luminosity GPIO pin set failed");
    k_sleep(K_MSEC(10)); /* Wait for the sensor to be ready */

    /* Read the value */
    RET_IF_ERR(adc_sequence_init_dt(&pt19_adc_spec, &sequence), "Luminosity ADC sequence init failed");
    RET_IF_ERR(adc_read(pt19_adc_spec.dev, &sequence), "Luminosity ADC read failed");

    /* Deactivate power to the sensor */
    RET_IF_ERR(gpio_pin_set_dt(&pt19_enable_spec, 0), "Luminosity GPIO pin set failed");

    /* Convert the value to a percentage */
    *luminosity = mapRange(sample_buffer, 0, 1023, 0, 100);

    LOG_DBG("Luminosity | raw: %d \t luminosity: %d.%d%%", sample_buffer, (int)*luminosity, (int)(*luminosity * 100) % 100);

    LOG_INF("luminosity read done");

    return 0;
}

/**
 * @brief Read the battery voltage in V
 * 
 * @param voltage Pointer to the voltage value
 * @return int 0 if success, error code otherwise
*/
int battery_voltage_read(float *voltage) {
    LOG_INF("battery voltage read");

    if(!isInisialized) {
        LOG_ERR("adc devices not initialized");
        return -1;
    }

    /* Read the value */
    RET_IF_ERR(adc_sequence_init_dt(&bat_adc_spec, &sequence), "Battery voltage ADC sequence init failed");
    RET_IF_ERR(adc_read(bat_adc_spec.dev, &sequence), "Battery voltage ADC read failed");

    uint32_t millivolts = sample_buffer;
    RET_IF_ERR(adc_raw_to_millivolts_dt(&bat_adc_spec, &millivolts), "Battery voltage ADC raw to millivolts failed");
    *voltage = millivolts / 1000.0f;

    LOG_DBG("Battery voltage | raw: %d \t voltage: %d.%dV", sample_buffer, (int)*voltage, (int)(*voltage * 100) % 100);

    LOG_INF("battery voltage read done");

    return 0;
}