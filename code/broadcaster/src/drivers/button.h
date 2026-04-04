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

/**
 * @brief Get the button pressed semaphore for event-driven wakeup
 *
 * Used by main loop to wait for button press with timeout:
 *   k_sem_take(&button_get_pressed_sem(), K_SECONDS(timeout_sec))
 *
 * The semaphore is given (incremented) by the button ISR on press.
 * 
 * @return struct k_sem* Pointer to the button pressed semaphore
*/
struct k_sem* button_get_pressed_sem(void);

#endif // BUTTON_H
