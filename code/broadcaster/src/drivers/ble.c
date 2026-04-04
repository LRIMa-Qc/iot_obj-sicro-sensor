/**
 * ble.c
 *
 * BLE driver
 *
 * Author: Nils Lahaye (2024)
 */

#include <errno.h>
#include <string.h>

#include "ble.h"
#include "../payload.h"
#include "../utils/app_settings.h"

LOG_MODULE_REGISTER(BLE_DRIVER, CONFIG_BLE_DRIVER_LOG_LEVEL);

static uint16_t counter;
static bool is_initialized;
static ble_state_t current_state = BLE_STATE_IDLE;

#if CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED
static uint16_t *sleep_time;
static struct bt_conn *active_conn;
#endif

static uint8_t service_data[25];
static bt_addr_le_t addr;
static struct bt_le_ext_adv *ext_adv;

static void ble_adv_timer_handler(struct k_timer *timer_id);
K_TIMER_DEFINE(advertising_timer, ble_adv_timer_handler, NULL);

#if CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED
static void ble_conn_timeout_timer_handler(struct k_timer *timer_id);
K_TIMER_DEFINE(conn_timeout_timer, ble_conn_timeout_timer_handler, NULL);

static void ble_conn_timeout_timer_handler(struct k_timer *timer_id)
{
	ARG_UNUSED(timer_id);
	LOG_WRN("Connection timed out (%ds)", CONFIG_BLE_CONN_TIMEOUT_SEC);
	current_state = BLE_STATE_TIMEOUT;

	if (active_conn == NULL) {
		LOG_WRN("No active connection to disconnect on timeout");
		current_state = BLE_STATE_IDLE;
		return;
	}

	current_state = BLE_STATE_DISCONNECT;
	int err = bt_conn_disconnect(active_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	if (err) {
		LOG_ERR("Unable to disconnect timed out connection (err %d)", err);
	}
}
#endif

static const struct bt_le_adv_param adv_param = {
	.options = BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_USE_IDENTITY
#if CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED
		| BT_LE_ADV_OPT_CONN
#endif
#if CONFIG_BLE_ADV_USE_CODED_PHY
		| BT_LE_ADV_OPT_CODED
#endif
	,
	.interval_min = ((uint16_t)(CONFIG_BLE_MIN_ADV_INTERVAL_MS) / 0.625f),
	.interval_max = ((uint16_t)(CONFIG_BLE_MAX_ADV_INTERVAL_MS) / 0.625f),
	.secondary_max_skip = 0U,
	.peer = NULL,
};

static int ble_state_transition(ble_state_t new_state)
{
	ble_state_t old_state = current_state;

	switch (old_state) {
	case BLE_STATE_IDLE:
		if (new_state != BLE_STATE_IDLE && new_state != BLE_STATE_ADVERTISING) {
			return -EINVAL;
		}
		break;
	case BLE_STATE_ADVERTISING:
		if (new_state != BLE_STATE_ADVERTISING && new_state != BLE_STATE_IDLE && new_state != BLE_STATE_CONNECTED) {
			return -EINVAL;
		}
		break;
	case BLE_STATE_CONNECTED:
		if (new_state != BLE_STATE_CONNECTED && new_state != BLE_STATE_TIMEOUT && new_state != BLE_STATE_DISCONNECT && new_state != BLE_STATE_IDLE) {
			return -EINVAL;
		}
		break;
	case BLE_STATE_TIMEOUT:
		if (new_state != BLE_STATE_TIMEOUT && new_state != BLE_STATE_DISCONNECT && new_state != BLE_STATE_IDLE) {
			return -EINVAL;
		}
		break;
	case BLE_STATE_DISCONNECT:
		if (new_state != BLE_STATE_DISCONNECT && new_state != BLE_STATE_IDLE) {
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	current_state = new_state;
	return 0;
}

void ble_adv_timer_handler(struct k_timer *timer_id)
{
	ARG_UNUSED(timer_id);
	/* Ignore advertising timeout once a connection has been established. */
	if (current_state == BLE_STATE_ADVERTISING) {
		current_state = BLE_STATE_IDLE;
	}
}

#if CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED
static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Connection failed (err %u)", err);
		return;
	}

	char bt_addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), bt_addr, sizeof(bt_addr));
	LOG_INF("Connected %s", bt_addr);

	if (active_conn != NULL) {
		bt_conn_unref(active_conn);
	}
	active_conn = bt_conn_ref(conn);

	k_timer_stop(&advertising_timer);
	current_state = BLE_STATE_CONNECTED;
	k_timer_start(&conn_timeout_timer, K_SECONDS(CONFIG_BLE_CONN_TIMEOUT_SEC), K_NO_WAIT);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);
	LOG_INF("Disconnected (reason %u)", reason);
	if (active_conn != NULL) {
		bt_conn_unref(active_conn);
		active_conn = NULL;
	}
	current_state = BLE_STATE_IDLE;
	k_timer_stop(&conn_timeout_timer);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static ssize_t write_sleep_time(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(attr);
	ARG_UNUSED(flags);

	char bt_addr[BT_ADDR_LE_STR_LEN] = "unknown";
	if (conn != NULL) {
		bt_addr_le_to_str(bt_conn_get_dst(conn), bt_addr, sizeof(bt_addr));
	}

	if (offset != 0U) {
		LOG_WRN("Sleep time write rejected from %s: invalid offset %u", bt_addr, offset);
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	if (len != sizeof(uint8_t) && len != sizeof(uint16_t)) {
		LOG_WRN("Sleep time write rejected from %s: invalid length %u (expected 1 or 2)", bt_addr,
			len);
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	if (sleep_time == NULL) {
		LOG_ERR("Sleep time write rejected from %s: storage pointer is NULL", bt_addr);
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	uint16_t parsed_value;
	if (len == sizeof(uint8_t)) {
		parsed_value = ((const uint8_t *)buf)[0];
	} else {
		memcpy(&parsed_value, buf, sizeof(parsed_value));
	}
	uint16_t old_value = *sleep_time;

	if (parsed_value < 1U || parsed_value > CONFIG_SENSOR_SLEEP_DURATION_MAX_SEC) {
		LOG_WRN("Sleep time write rejected from %s: value %u outside [1..%u]", bt_addr,
			parsed_value, CONFIG_SENSOR_SLEEP_DURATION_MAX_SEC);
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	*sleep_time = parsed_value;
	LOG_INF("Sleep time updated by %s: %u -> %u sec", bt_addr, old_value, parsed_value);
	k_timer_start(&conn_timeout_timer, K_SECONDS(CONFIG_BLE_CONN_TIMEOUT_SEC), K_NO_WAIT);
	return len;
}

static ssize_t read_sleep_time(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				 void *buf, uint16_t len, uint16_t offset)
{
	ARG_UNUSED(attr);

	char bt_addr[BT_ADDR_LE_STR_LEN] = "unknown";
	if (conn != NULL) {
		bt_addr_le_to_str(bt_conn_get_dst(conn), bt_addr, sizeof(bt_addr));
	}

	if (sleep_time == NULL) {
		LOG_ERR("Sleep time read rejected from %s: storage pointer is NULL", bt_addr);
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	LOG_INF("Sleep time read by %s: %u sec", bt_addr, *sleep_time);
	k_timer_start(&conn_timeout_timer, K_SECONDS(CONFIG_BLE_CONN_TIMEOUT_SEC), K_NO_WAIT);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, sleep_time, sizeof(*sleep_time));
}

BT_GATT_SERVICE_DEFINE(sleep_time_service,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_16(SLEEP_TIME_SERVICE_UUID)),
	BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(SLEEP_TIME_CHARACTERISTIC_UUID),
		BT_GATT_CHRC_WRITE | BT_GATT_CHRC_READ,
		BT_GATT_PERM_WRITE | BT_GATT_PERM_READ,
		read_sleep_time, write_sleep_time, &sleep_time),
);
#endif

static void ble_start_adv(int err)
{
	const char *device_name = app_settings_get_device_name();
	size_t device_name_len = strlen(device_name);

	if (device_name_len == 0U) {
		LOG_WRN("Persisted device name is empty, using build default");
		device_name = CONFIG_BLE_USER_DEFINED_NAME;
		device_name_len = strlen(device_name);
	}

	if (err) {
		LOG_ERR("Bluetooth failed to enable (err %d)", err);
		current_state = BLE_STATE_IDLE;
		return;
	}

	LOG_INF("Starting advertising with name: %s", device_name);
	LOG_IF_ERR(bt_set_name(device_name), "Unable to set broadcaster device name");

	LOG_IF_ERR(bt_le_ext_adv_create(&adv_param, NULL, &ext_adv), "Advertising failed to create");

	service_data[0] = BROADCAST_SERVICE_UUID_1;
	service_data[1] = BROADCAST_SERVICE_UUID_2;
	service_data[2] = (uint8_t)(counter & 0xFFU);
	service_data[3] = (uint8_t)((counter >> 8) & 0xFFU);

	const struct bt_data adv_data[] = {
#if CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED
		BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
		BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(SLEEP_TIME_SERVICE_UUID)),
#endif
		BT_DATA(BT_DATA_NAME_COMPLETE, device_name, device_name_len),
		BT_DATA(BT_DATA_SVC_DATA16, service_data, sizeof(service_data)),
	};

	LOG_IF_ERR(bt_le_ext_adv_set_data(ext_adv, adv_data, ARRAY_SIZE(adv_data), NULL, 0),
		   "Advertising failed to set data");

	counter++;
	current_state = BLE_STATE_ADVERTISING;
	k_timer_start(&advertising_timer, K_SECONDS(CONFIG_BLE_ADV_DURATION_SEC), K_NO_WAIT);

	LOG_IF_ERR(bt_le_ext_adv_start(ext_adv, BT_LE_EXT_ADV_START_DEFAULT), "Advertising failed to start");
	LOG_INF("Advertising started");
}

int ble_init(uint16_t *sleep_time_ptr)
{
	if (is_initialized) {
		LOG_WRN("BLE already initialized");
		return 0;
	}

	k_timer_init(&advertising_timer, ble_adv_timer_handler, NULL);
#if CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED
	k_timer_init(&conn_timeout_timer, ble_conn_timeout_timer_handler, NULL);
	active_conn = NULL;
	if (sleep_time_ptr == NULL) {
		LOG_ERR("Sleep time pointer is NULL");
		return -EINVAL;
	}
	sleep_time = sleep_time_ptr;
#endif

	const char *device_mac = app_settings_get_device_mac();
	LOG_INF("Using device MAC: %s", device_mac);
	RET_IF_ERR(bt_addr_le_from_str(device_mac, "random", &addr),
		   "Unable to convert MAC address");
	int id = bt_id_create(&addr, NULL);
	if (id != 0) {
		LOG_ERR("Unable to set MAC address");
		return id;
	}

	is_initialized = true;
	LOG_INF("Bluetooth initialized");
	return 0;
}

int ble_stop_adv(void)
{
	if (!is_initialized) {
		return -EINVAL;
	}

	if (ext_adv != NULL) {
		LOG_IF_ERR(bt_le_ext_adv_stop(ext_adv), "Unable to stop advertising set");
	}

	return 0;
}

int ble_encode_adv_data(sensors_data_t *sensors_data)
{
	if (sensors_data == NULL) {
		return -EINVAL;
	}

	service_data[0] = BROADCAST_SERVICE_UUID_1;
	service_data[1] = BROADCAST_SERVICE_UUID_2;
	service_data[2] = (uint8_t)(counter & 0xFFU);
	service_data[3] = (uint8_t)((counter >> 8) & 0xFFU);

	return payload_encode(sensors_data, service_data, sizeof(service_data));
}

int ble_adv(void)
{
	if (!is_initialized) {
		return -EINVAL;
	}

	LOG_IF_ERR(bt_enable(ble_start_adv), "Unable to enable bluetooth");

	while (current_state == BLE_STATE_ADVERTISING) {
		k_sleep(K_MSEC(10));
	}

#if CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED
	while (current_state == BLE_STATE_CONNECTED || current_state == BLE_STATE_TIMEOUT ||
	       current_state == BLE_STATE_DISCONNECT) {
		k_sleep(K_MSEC(10));
	}
#endif

	LOG_IF_ERR(ble_stop_adv(), "Unable to stop advertising");
	LOG_IF_ERR(bt_disable(), "Unable to disable bluetooth");
	k_timer_stop(&advertising_timer);
#if CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED
	k_timer_stop(&conn_timeout_timer);
#endif
	led1_off();
	return 0;
}

int ble_transition_to_state(ble_state_t new_state)
{
	return ble_state_transition(new_state);
}

ble_state_t ble_get_state(void)
{
	return current_state;
}
