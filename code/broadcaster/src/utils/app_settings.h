/**
 * app_settings.h
 *
 * Application settings persistence helpers.
 */

#ifndef APP_SETTINGS_H_
#define APP_SETTINGS_H_

#include <stdint.h>

int app_settings_init(uint16_t *sleep_time_ptr);
int app_settings_persist_sleep_time_if_changed(void);

const char *app_settings_get_device_name(void);
const char *app_settings_get_device_mac(void);

#endif /* APP_SETTINGS_H_ */
