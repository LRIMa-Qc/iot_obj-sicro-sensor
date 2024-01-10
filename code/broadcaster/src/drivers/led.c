/**
 * led.c
 * 
 * This file contains the code for the led driver
 * 
 * Author: Nils Lahaye (2024)
*/

#include "led.h"

LOG_MODULE_REGISTER(LED, CONFIG_LED_LOG_LEVEL);

#define LED1_NODE DT_ALIAS(led1)
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);

static bool is_init = false;

static bool led1_state = false;

int led_init(void) {
    LOG_INF("Initializing led driver");

    if(!device_is_ready(led1.port)) {
        LOG_ERR("LED1 is not ready");
        return -1;
    }

    gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);

    is_init = true;

    led1_off();

    LOG_INF("Led driver initialized");

    return 0;
}

bool led1_get_state(void) {
    return led1_state;
}

int led1_set_state(bool state) {
    if(!is_init) {
        LOG_ERR("Led driver is not initialized");
        return -1;
    }

    gpio_pin_set_dt(&led1, state);

    led1_state = state;

    LOG_INF("LED1 state set to %d", state);

    return 0;
}

int led1_on(void) {
    return led1_set_state(true);
}

int led1_off(void) {
    return led1_set_state(false);
}

int led1_toggle(void) {
    return led1_set_state(!led1_state);
}

