/**
 * ble.c
 *
 * BLE driver
 *
 * Author: Nils Lahaye (2024)
 */

#include <errno.h>
#include <string.h>

#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>

#include "ble.h"
#include "../payload.h"
#include "../utils/app_settings.h"

LOG_MODULE_REGISTER(BLE_DRIVER, CONFIG_BLE_DRIVER_LOG_LEVEL);

static uint16_t counter;
static bool is_initialized;
static ble_state_t current_state = BLE_STATE_IDLE;
static uint16_t *g_sleep_time_ptr;
static bool g_has_pending_sleep_update;
static uint16_t g_pending_sleep_update;
static atomic_t g_abort_ble_activity;

static bt_addr_le_t g_gateway_addr;
static bool g_has_gateway_addr;
static uint16_t g_device_node_id;

static uint8_t service_data[PAYLOAD_SIZE];
static bt_addr_le_t addr;

static void ble_adv_timer_handler(struct k_timer *timer_id);
K_TIMER_DEFINE(advertising_timer, ble_adv_timer_handler, NULL);

static const struct bt_le_adv_param adv_param = {
	.options = BT_LE_ADV_OPT_USE_IDENTITY | BT_LE_ADV_OPT_SCANNABLE,
	.interval_min = ((uint16_t)(CONFIG_BLE_MIN_ADV_INTERVAL_MS) / 0.625f),
	.interval_max = ((uint16_t)(CONFIG_BLE_MAX_ADV_INTERVAL_MS) / 0.625f),
	.peer = NULL,
};

static const struct bt_le_scan_param scan_param = {
	.type = BT_LE_SCAN_TYPE_ACTIVE,
	.options = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
	.interval = 0x0010,
	.window = 0x0010,
};

#define DOWNLINK_SCAN_WINDOW_MS 200
#define DOWNLINK_PAYLOAD_SIZE 8
#define SVC_DATA16_UUID_PREFIX_SIZE 2

struct downlink_parse_ctx {
	uint32_t sleep_duration_sec;
	bool matched;
};

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

static bool is_gateway_address_match(const bt_addr_le_t *scan_addr)
{
	if (!g_has_gateway_addr) {
		return true;
	}

	return memcmp(scan_addr->a.val, g_gateway_addr.a.val, sizeof(scan_addr->a.val)) == 0;
}

static bool parse_downlink_payload(const uint8_t *data, uint8_t len, uint32_t *sleep_duration_sec_out)
{
	if (data == NULL || sleep_duration_sec_out == NULL || len < DOWNLINK_PAYLOAD_SIZE) {
		return false;
	}

	uint16_t company_id = sys_get_le16(&data[0]);
	if (company_id != PAYLOAD_COMPANY_ID) {
		return false;
	}

	uint16_t target_node_id = sys_get_le16(&data[2]);
	if (target_node_id != g_device_node_id) {
		return false;
	}

	*sleep_duration_sec_out = sys_get_le32(&data[4]);
	return true;
}

static bool parse_downlink_from_ad_field(const struct bt_data *ad, uint32_t *sleep_duration_sec_out)
{
	if (ad == NULL) {
		return false;
	}

	if (ad->type == BT_DATA_MANUFACTURER_DATA) {
		return parse_downlink_payload(ad->data, ad->data_len, sleep_duration_sec_out);
	}

	if (ad->type == BT_DATA_SVC_DATA16) {
		if (parse_downlink_payload(ad->data, ad->data_len, sleep_duration_sec_out)) {
			return true;
		}

		if (ad->data_len >= (DOWNLINK_PAYLOAD_SIZE + SVC_DATA16_UUID_PREFIX_SIZE)) {
			return parse_downlink_payload(ad->data + SVC_DATA16_UUID_PREFIX_SIZE,
						      ad->data_len - SVC_DATA16_UUID_PREFIX_SIZE,
						      sleep_duration_sec_out);
		}
	}

	return false;
}

static bool downlink_ad_parse_cb(struct bt_data *data, void *user_data)
{
	struct downlink_parse_ctx *ctx = user_data;

	if (ctx == NULL) {
		return false;
	}

	if (data->type != BT_DATA_MANUFACTURER_DATA && data->type != BT_DATA_SVC_DATA16) {
		return true;
	}

	uint32_t sleep_duration_sec = 0U;
	if (!parse_downlink_from_ad_field(data, &sleep_duration_sec)) {
		return true;
	}

	LOG_INF("Got downlink with sleep duration: %u sec", sleep_duration_sec);
	ctx->sleep_duration_sec = sleep_duration_sec;
	ctx->matched = true;
	return false;
}

static void ble_scan_recv_cb(const bt_addr_le_t *scan_addr, int8_t rssi, uint8_t adv_type,
			    struct net_buf_simple *buf)
{
	ARG_UNUSED(rssi);
	ARG_UNUSED(adv_type);

	if (!is_gateway_address_match(scan_addr)) {
		return;
	}

	struct downlink_parse_ctx ctx = {0};
	bt_data_parse(buf, downlink_ad_parse_cb, &ctx);

	if (!ctx.matched || g_sleep_time_ptr == NULL) {
		return;
	}

	uint32_t raw_sleep = ctx.sleep_duration_sec;
	if (raw_sleep == 0U) {
		return;
	}

	if (raw_sleep > UINT16_MAX) {
		raw_sleep = UINT16_MAX;
	}

	g_pending_sleep_update = clamp_sleep_time_value((uint16_t)raw_sleep);
	g_has_pending_sleep_update = true;
	atomic_set(&g_abort_ble_activity, 1);

	if (g_sleep_time_ptr != NULL) {
		LOG_INF("Downlink accepted: sleep %u -> %u sec", *g_sleep_time_ptr,
			g_pending_sleep_update);
	} else {
		LOG_INF("Downlink accepted: sleep update set to %u sec", g_pending_sleep_update);
	}
}

static int ble_scan_downlink_window(uint32_t duration_ms)
{
	LOG_DBG("Starting downlink scan window for %u ms", duration_ms);
	int err = bt_le_scan_start(&scan_param, ble_scan_recv_cb);
	if (err) {
		LOG_WRN("Unable to start downlink scan window (%d)", err);
		return err;
	}

	uint32_t elapsed_ms = 0U;
	while (elapsed_ms < duration_ms) {
		if (atomic_get(&g_abort_ble_activity) != 0) {
			LOG_INF("Stopping BLE activity early after downlink sleep update");
			break;
		}

		uint32_t chunk_ms = (duration_ms - elapsed_ms > 10U) ? 10U : (duration_ms - elapsed_ms);
		k_sleep(K_MSEC(chunk_ms));
		elapsed_ms += chunk_ms;
	}

	err = bt_le_scan_stop();
	if (err) {
		LOG_WRN("Unable to stop downlink scan window (%d)", err);
	}

	if (atomic_get(&g_abort_ble_activity) != 0 && current_state == BLE_STATE_ADVERTISING) {
		int adv_stop_err = bt_le_adv_stop();
		if (adv_stop_err) {
			LOG_WRN("Unable to stop advertising after downlink update (%d)", adv_stop_err);
		} else {
			current_state = BLE_STATE_IDLE;
		}
	}

	LOG_DBG("Downlink scan window ended");
	return 0;
}

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
		if (new_state != BLE_STATE_ADVERTISING && new_state != BLE_STATE_IDLE &&
		    new_state != BLE_STATE_SCANNING) {
			return -EINVAL;
		}
		break;
	case BLE_STATE_SCANNING:
		if (new_state != BLE_STATE_SCANNING && new_state != BLE_STATE_IDLE) {
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
	if (current_state == BLE_STATE_ADVERTISING) {
		current_state = BLE_STATE_IDLE;
	}
}

static void ble_start_adv(int err)
{
	const char *device_name = app_settings_get_device_name();
	size_t device_name_len = strlen(device_name);

	if (device_name_len == 0U) {
		LOG_WRN("Persisted device name is empty, using build default");
		device_name = BLE_NAME;
		device_name_len = strlen(device_name);
	}

	if (err) {
		LOG_ERR("Bluetooth failed to enable (err %d)", err);
		current_state = BLE_STATE_IDLE;
		return;
	}

	LOG_INF("Starting advertising with name: %s", device_name);
	LOG_IF_ERR(bt_set_name(device_name), "Unable to set broadcaster device name");

	const struct bt_data adv_data[] = {
		BT_DATA(BT_DATA_SVC_DATA16, service_data, sizeof(service_data)),
	};

	const struct bt_data scan_resp[] = {
		BT_DATA(BT_DATA_NAME_COMPLETE, device_name, device_name_len),
	};

	current_state = BLE_STATE_ADVERTISING;
	k_timer_start(&advertising_timer, K_SECONDS(CONFIG_BLE_ADV_DURATION_SEC), K_NO_WAIT);

	int adv_err = bt_le_adv_start(&adv_param, adv_data, ARRAY_SIZE(adv_data), scan_resp,
				      ARRAY_SIZE(scan_resp));
	if (adv_err) {
		LOG_ERR("Advertising failed to start (err %d)", adv_err);
		k_timer_stop(&advertising_timer);
		current_state = BLE_STATE_IDLE;
		return;
	}

	LOG_INF("Advertising started");
}

int ble_init(uint16_t *sleep_time_ptr)
{
	if (is_initialized) {
		LOG_WRN("BLE already initialized");
		return 0;
	}

	k_timer_init(&advertising_timer, ble_adv_timer_handler, NULL);
	g_sleep_time_ptr = sleep_time_ptr;
	g_has_pending_sleep_update = false;
	g_device_node_id = (uint16_t)app_settings_get_device_node_id();
	atomic_set(&g_abort_ble_activity, 0);

	const char *device_mac = app_settings_get_device_mac();
	LOG_INF("Using device MAC: %s", device_mac);
	RET_IF_ERR(bt_addr_le_from_str(device_mac, "random", &addr),
		   "Unable to convert MAC address");
	int id = bt_id_create(&addr, NULL);
	if (id < 0) {
		LOG_ERR("Unable to set MAC address (err %d)", id);
		return id;
	}
	LOG_INF("Using device node ID: %u", g_device_node_id);
	LOG_INF("Created BLE identity index: %d", id);

	if (strlen(CONFIG_BLE_GATEWAY_MAC_ADDR) > 0U) {
		int gateway_err = bt_addr_le_from_str(CONFIG_BLE_GATEWAY_MAC_ADDR, "random",
					    &g_gateway_addr);
		if (!gateway_err) {
			g_has_gateway_addr = true;
			LOG_INF("Gateway downlink filter enabled: %s", CONFIG_BLE_GATEWAY_MAC_ADDR);
		} else {
			LOG_WRN("Invalid CONFIG_BLE_GATEWAY_MAC_ADDR, filter disabled");
			g_has_gateway_addr = false;
		}
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

	LOG_IF_ERR(bt_le_adv_stop(), "Unable to stop advertising set");

	return 0;
}

int ble_encode_adv_data(sensors_data_t *sensors_data, uint8_t present_mask,
		       uint32_t sleep_duration_sec)
{
	if (sensors_data == NULL) {
		return -EINVAL;
	}

	service_data[0] = (uint8_t)(PAYLOAD_COMPANY_ID & 0xFFU);
	service_data[1] = (uint8_t)((PAYLOAD_COMPANY_ID >> 8) & 0xFFU);
	service_data[2] = (uint8_t)(g_device_node_id & 0xFFU);
	service_data[3] = (uint8_t)((g_device_node_id >> 8) & 0xFFU);
	service_data[4] = (uint8_t)(counter & 0xFFU);

	int err = payload_encode(sensors_data, service_data, sizeof(service_data), present_mask,
				 sleep_duration_sec);
	if (err) {
		return err;
	}

	counter = (uint16_t)((counter + 1U) & 0xFFU);
	return 0;
}

int ble_adv(void)
{
	if (!is_initialized) {
		return -EINVAL;
	}

	LOG_IF_ERR(bt_enable(ble_start_adv), "Unable to enable bluetooth");
	atomic_set(&g_abort_ble_activity, 0);

	uint32_t adv_start_wait_ms = 0U;
	while (current_state == BLE_STATE_IDLE && adv_start_wait_ms < 1000U) {
		k_sleep(K_MSEC(10));
		adv_start_wait_ms += 10U;
	}

	if (current_state == BLE_STATE_ADVERTISING) {
		uint32_t scan_duration_ms = (CONFIG_BLE_ADV_DURATION_SEC * 1000U) +
					   DOWNLINK_SCAN_WINDOW_MS;
		LOG_IF_ERR(ble_scan_downlink_window(scan_duration_ms),
			   "Unable to run downlink scan window");
	} else {
		LOG_WRN("Advertising did not start, skipping concurrent downlink scan");
	}

	while (current_state == BLE_STATE_ADVERTISING) {
		k_sleep(K_MSEC(10));
	}

	if (current_state == BLE_STATE_ADVERTISING) {
		LOG_IF_ERR(ble_stop_adv(), "Unable to stop advertising");
	}
	LOG_IF_ERR(bt_disable(), "Unable to disable bluetooth");
	k_timer_stop(&advertising_timer);
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

bool ble_take_pending_sleep_update(uint16_t *sleep_time_sec_out)
{
	if (sleep_time_sec_out == NULL || !g_has_pending_sleep_update) {
		return false;
	}

	*sleep_time_sec_out = g_pending_sleep_update;
	g_has_pending_sleep_update = false;
	return true;
}
