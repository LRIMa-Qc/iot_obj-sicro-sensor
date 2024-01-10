/**
 * button.c
 * 
 * This file contains the code for the button driver
 * 
 * Author: Nils Lahaye (2024)
*/

#include "button.h"

LOG_MODULE_REGISTER(BUTTON, CONFIG_BUTTON_LOG_LEVEL);

static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET(DT_ALIAS(button1), gpios);

static bool is_init = false;

static int button1_state = 0;


void button1_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    button1_read();

    LOG_INF("Button1 state changed to %d", button1_state);
}

static struct gpio_callback button1_cb_data;

int button_init(void) {
    LOG_INF("Initializing button driver");

    // Check if the button1 is correctly defined in the devicetree
    if (!device_is_ready(button1.port)) {
        LOG_ERR("Button1 is not ready");
        return -1;
    }

    // Configure the button1 pin
    LOG_IF_ERR(gpio_pin_configure_dt(&button1, GPIO_INPUT), "Unable to configure button1 pin");

    gpio_pin_interrupt_configure_dt(&button1, GPIO_INT_EDGE_BOTH);

    gpio_init_callback(&button1_cb_data, button1_cb, BIT(button1.pin));

    LOG_IF_ERR(gpio_add_callback(button1.port, &button1_cb_data), "Unable to add button1 callback");

    is_init = true;

    button1_read(); // Read the button1 state

    LOG_INF("Button driver initialized");

    return 0;
}

int button1_read(void) {
    if(!is_init) {
        LOG_ERR("Button driver is not initialized");
        return -1;
    }

    LOG_INF("Reading button1 state");

    button1_state = gpio_pin_get_dt(&button1);

    return button1_state;
}

int get_button1_state(void) { return button1_state; }