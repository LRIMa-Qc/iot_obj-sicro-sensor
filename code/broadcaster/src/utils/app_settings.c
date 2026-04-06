/**
 * app_settings.c
 *
 * Application settings persistence helpers.
 */

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include "app_settings.h"
#include "../utils.h"

LOG_MODULE_REGISTER(APP_SETTINGS, CONFIG_APP_SETTINGS_LOG_LEVEL);

#define APP_SETTINGS_SUBTREE "app"
#define APP_SETTINGS_KEY_SLEEP_TIME "sleep_time"
#define APP_SETTINGS_KEY_DEVICE_NAME "device_name"
#define APP_SETTINGS_KEY_DEVICE_MAC "device_mac"
#define APP_SETTINGS_KEY_DEVICE_NODE_ID "device_node_id"

#define APP_SETTINGS_PATH_SLEEP_TIME APP_SETTINGS_SUBTREE "/" APP_SETTINGS_KEY_SLEEP_TIME
#define APP_SETTINGS_PATH_DEVICE_NAME APP_SETTINGS_SUBTREE "/" APP_SETTINGS_KEY_DEVICE_NAME
#define APP_SETTINGS_PATH_DEVICE_MAC APP_SETTINGS_SUBTREE "/" APP_SETTINGS_KEY_DEVICE_MAC
#define APP_SETTINGS_PATH_DEVICE_NODE_ID APP_SETTINGS_SUBTREE "/" APP_SETTINGS_KEY_DEVICE_NODE_ID

static uint16_t *g_sleep_time_ptr;
static uint16_t g_persisted_sleep_time;

static char g_device_name[sizeof(CONFIG_BLE_USER_DEFINED_NAME)];
static char g_device_mac[sizeof(CONFIG_BLE_USER_DEFINED_MAC_ADDR)];
static uint8_t g_device_node_id = CONFIG_BLE_NODE_ID;

static bool g_has_persisted_sleep_time;
static bool g_has_persisted_device_name;
static bool g_has_persisted_device_mac;
static bool g_has_persisted_device_node_id;
static bool g_initialized;

static uint16_t clamp_sleep_time_value(uint16_t value)
{
	if (value < 1U) {
		return 1U;
	}

#if CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED
	if (value > CONFIG_SENSOR_SLEEP_DURATION_MAX_SEC) {
		return CONFIG_SENSOR_SLEEP_DURATION_MAX_SEC;
	}
#endif

	return value;
}

static int read_persisted_string(settings_read_cb read_cb, void *cb_arg, size_t len_rd,
				 char *dst, size_t dst_size, const char *label)
{
	if (len_rd == 0U || len_rd > dst_size) {
		LOG_WRN("Invalid stored %s size: %u", label, (unsigned int)len_rd);
		return -EINVAL;
	}

	int rc = read_cb(cb_arg, dst, len_rd);
	if (rc < 0) {
		LOG_ERR("Unable to read stored %s (%d)", label, rc);
		return rc;
	}

	if ((size_t)rc != len_rd) {
		LOG_WRN("Stored %s size mismatch: read %d of %u", label, rc,
			(unsigned int)len_rd);
		return -EINVAL;
	}

	if (len_rd == dst_size) {
		dst[dst_size - 1U] = '\0';
	} else {
		dst[len_rd] = '\0';
	}

	return 0;
}

static int app_settings_set(const char *name, size_t len_rd,
			    settings_read_cb read_cb, void *cb_arg)
{
	if (strcmp(name, APP_SETTINGS_KEY_SLEEP_TIME) == 0) {
		uint16_t loaded_value;
		if (len_rd != sizeof(loaded_value)) {
			LOG_WRN("Invalid stored sleep_time size: %u", (unsigned int)len_rd);
			return -EINVAL;
		}

		int rc = read_cb(cb_arg, &loaded_value, sizeof(loaded_value));
		if (rc < 0) {
			LOG_ERR("Unable to read stored sleep_time (%d)", rc);
			return rc;
		}

		if ((size_t)rc != sizeof(loaded_value)) {
			LOG_WRN("Stored sleep_time size mismatch: read %d of %u", rc,
				(unsigned int)sizeof(loaded_value));
			return -EINVAL;
		}

		g_persisted_sleep_time = clamp_sleep_time_value(loaded_value);
		if (g_sleep_time_ptr != NULL) {
			*g_sleep_time_ptr = g_persisted_sleep_time;
		}
		g_has_persisted_sleep_time = true;

		LOG_INF("Loaded persisted sleep_time: %u", g_persisted_sleep_time);
		return 0;
	}

	if (strcmp(name, APP_SETTINGS_KEY_DEVICE_NAME) == 0) {
		int rc = read_persisted_string(read_cb, cb_arg, len_rd, g_device_name,
					      sizeof(g_device_name), "device_name");
		if (rc == 0) {
			g_has_persisted_device_name = true;
			LOG_INF("Loaded persisted device_name: %s", g_device_name);
		}
		return rc;
	}

	if (strcmp(name, APP_SETTINGS_KEY_DEVICE_MAC) == 0) {
		int rc = read_persisted_string(read_cb, cb_arg, len_rd, g_device_mac,
					      sizeof(g_device_mac), "device_mac");
		if (rc == 0) {
			g_has_persisted_device_mac = true;
			LOG_INF("Loaded persisted device_mac: %s", g_device_mac);
		}
		return rc;
	}

	if (strcmp(name, APP_SETTINGS_KEY_DEVICE_NODE_ID) == 0) {
		uint8_t loaded_value;
		if (len_rd != sizeof(loaded_value)) {
			LOG_WRN("Invalid stored device_node_id size: %u", (unsigned int)len_rd);
			return -EINVAL;
		}

		int rc = read_cb(cb_arg, &loaded_value, sizeof(loaded_value));
		if (rc < 0) {
			LOG_ERR("Unable to read stored device_node_id (%d)", rc);
			return rc;
		}

		if ((size_t)rc != sizeof(loaded_value)) {
			LOG_WRN("Stored device_node_id size mismatch: read %d of %u", rc,
				(unsigned int)sizeof(loaded_value));
			return -EINVAL;
		}

		g_device_node_id = loaded_value;
		g_has_persisted_device_node_id = true;
		LOG_INF("Loaded persisted device_node_id: %u", g_device_node_id);
		return 0;
	}

	return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(app_settings_handler, APP_SETTINGS_SUBTREE, NULL,
			       app_settings_set, NULL, NULL);

static int persist_missing_defaults(void)
{
	if (!g_has_persisted_sleep_time) {
		LOG_WRN("No persisted sleep_time found, using Kconfig default: %u", *g_sleep_time_ptr);
		g_persisted_sleep_time = clamp_sleep_time_value(*g_sleep_time_ptr);
		*g_sleep_time_ptr = g_persisted_sleep_time;
		RET_IF_ERR(settings_save_one(APP_SETTINGS_PATH_SLEEP_TIME, g_sleep_time_ptr,
				     sizeof(*g_sleep_time_ptr)),
			   "Unable to save default sleep_time setting");
		LOG_INF("Persisted default sleep_time: %u", *g_sleep_time_ptr);
	}

	if (!g_has_persisted_device_name) {
		LOG_WRN("No persisted device_name found, using Kconfig default: %s", g_device_name);
		RET_IF_ERR(settings_save_one(APP_SETTINGS_PATH_DEVICE_NAME, g_device_name,
				     strlen(g_device_name) + 1U),
			   "Unable to save default device_name setting");
		LOG_INF("Persisted default device_name: %s", g_device_name);
	}

	if (!g_has_persisted_device_mac) {
		LOG_WRN("No persisted device_mac found, using Kconfig default: %s", g_device_mac);
		RET_IF_ERR(settings_save_one(APP_SETTINGS_PATH_DEVICE_MAC, g_device_mac,
				     strlen(g_device_mac) + 1U),
			   "Unable to save default device_mac setting");
		LOG_INF("Persisted default device_mac: %s", g_device_mac);
	}

	if (!g_has_persisted_device_node_id) {
		LOG_WRN("No persisted device_node_id found, using Kconfig default: %u", 
			g_device_node_id);
		RET_IF_ERR(settings_save_one(APP_SETTINGS_PATH_DEVICE_NODE_ID, &g_device_node_id,
				     sizeof(g_device_node_id)),
			   "Unable to save default device_node_id setting");
		LOG_INF("Persisted default device_node_id: %u", g_device_node_id);
	}

	return 0;
}

int app_settings_init(uint16_t *sleep_time_ptr)
{
	if (g_initialized) {
		return 0;
	}

	if (sleep_time_ptr == NULL) {
		return -EINVAL;
	}

	g_sleep_time_ptr = sleep_time_ptr;
	*g_sleep_time_ptr = clamp_sleep_time_value(*g_sleep_time_ptr);
	g_persisted_sleep_time = *g_sleep_time_ptr;

	strncpy(g_device_name, CONFIG_BLE_USER_DEFINED_NAME, sizeof(g_device_name) - 1U);
	g_device_name[sizeof(g_device_name) - 1U] = '\0';

	strncpy(g_device_mac, CONFIG_BLE_USER_DEFINED_MAC_ADDR, sizeof(g_device_mac) - 1U);
	g_device_mac[sizeof(g_device_mac) - 1U] = '\0';

	RET_IF_ERR(settings_subsys_init(), "Unable to initialize settings subsystem");

	int rc = settings_load_subtree(APP_SETTINGS_SUBTREE);
	if (rc) {
		LOG_ERR("Unable to load app settings (%d)", rc);
		return rc;
	}

	RET_IF_ERR(persist_missing_defaults(), "Unable to persist default app settings");

	g_initialized = true;
	LOG_INF("Application settings initialized");
	return 0;
}

int app_settings_persist_sleep_time_if_changed(void)
{
	if (g_sleep_time_ptr == NULL) {
		return -EINVAL;
	}

	*g_sleep_time_ptr = clamp_sleep_time_value(*g_sleep_time_ptr);
	if (*g_sleep_time_ptr == g_persisted_sleep_time) {
		LOG_DBG("Sleep time unchanged, skipping persistence (%u sec)", *g_sleep_time_ptr);
		return 0;
	}

	LOG_INF("Sleep time changed in RAM: %u -> %u sec", g_persisted_sleep_time,
		*g_sleep_time_ptr);
	RET_IF_ERR(settings_save_one(APP_SETTINGS_PATH_SLEEP_TIME, g_sleep_time_ptr,
			     sizeof(*g_sleep_time_ptr)),
		   "Unable to save sleep_time setting");

	g_persisted_sleep_time = *g_sleep_time_ptr;
	LOG_INF("Persisted sleep_time: %u", g_persisted_sleep_time);
	return 0;
}

const char *app_settings_get_device_name(void)
{
	return g_device_name;
}

const char *app_settings_get_device_mac(void)
{
	return g_device_mac;
}

uint8_t app_settings_get_device_node_id(void)
{
	return g_device_node_id;
}
