/**
 * led.h
 * 
 * LED driver
 * 
 * Author: Nils Lahaye (2024)
*/

#ifndef LED_H
#define LED_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include "../utils.h"

/**
 * @brief Initialize the LED driver
 * 
 * @return int 0 if success, -1 if error
*/
int led_init(void);

/**
 * @brief Get the LED1 state
 * 
 * @return true if on, false if off
*/
bool led1_get_state(void);

/**
 * @brief Set the LED1 state
 * 
 * @param state true to turn on, false to turn off
 * @return int 0 if success, -1 if error
*/
int led1_set_state(bool state);

/**
 * @brief Turn on the LED1
 * 
 * @return int 0 if success, -1 if error
*/
int led1_on(void);

/**
 * @brief Turn off the LED1
 * 
 * @return int 0 if success, -1 if error
*/
int led1_off(void);

/**
 * @brief Toggle the LED1
 * 
 * @return int 0 if success, -1 if error
*/
int led1_toggle(void);

#endif /* LED_H */
