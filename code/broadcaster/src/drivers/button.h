/**
 * button.h
 * 
 * Button driver
 * 
 * This driver is used to read the state of the button.
 * 
 * Author: Nils Lahaye (2024)
*/

#ifndef BUTTON_H
#define BUTTON_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include "../utils.h"

/**
 * @brief Initialize the button driver
 * 
 * @return int 0 if success
*/
int button_init(void);

/**
 * @brief Read the state of the button1
 * 
 * @return int 1 if button is pressed, 0 if not
*/
int button1_read(void);

/**
 * @brief Get the state of the button1
 * 
 * @return int 1 if button is pressed, 0 if not
*/
int get_button1_state(void);

#endif // BUTTON_H
