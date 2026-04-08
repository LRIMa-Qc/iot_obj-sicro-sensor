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

static struct k_timer led1_blink_timer;
static bool led1_blink_active = false;

#define LED_INIT_BLINK_HALF_PERIOD_MS 250U

static void led1_blink_timer_handler(struct k_timer *timer_id) {
    ARG_UNUSED(timer_id);
    (void)led1_toggle();
}

int led_init(void) {
    LOG_INF("Initializing led driver");

    if(!device_is_ready(led1.port)) {
        LOG_ERR("LED1 is not ready");
        return -1;
    }

    RET_IF_ERR(gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE), "Unable to configure LED1 pin");

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

    RET_IF_ERR(gpio_pin_set_dt(&led1, state), "Unable to set LED1 pin state");

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

int led_blink_start(void) {
    if(!is_init) {
        LOG_ERR("Led driver is not initialized");
        return -1;
    }

    k_timer_init(&led1_blink_timer, led1_blink_timer_handler, NULL);
    k_timer_start(&led1_blink_timer, K_NO_WAIT, K_MSEC(LED_INIT_BLINK_HALF_PERIOD_MS));
    led1_blink_active = true;
    return 0;
}

int led_blink_stop(void) {
    if (led1_blink_active) {
        k_timer_stop(&led1_blink_timer);
        led1_blink_active = false;
    }

    return 0;
}

