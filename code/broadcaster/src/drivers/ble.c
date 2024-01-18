/**
 * ble.c
 * 
 * BLE driver
 * 
 * Author: Nils Lahaye (2024)
 * 
*/

#include "ble.h"

LOG_MODULE_REGISTER(BLE_DRIVER, CONFIG_BLE_DRIVER_LOG_LEVEL);

/** Counter for the packet number (0-255)*/
static int counter;

/** Flag to check if the driver is initialized */
static bool isInisialized = false;

/** Flag to check if the adv time is done*/
static bool adv_time_done = true;

/** Function for when the timer is done*/
void ble_adv_timer_handler(struct k_timer *timer_id) { adv_time_done = true; }
/** Timer for the advertising duration */
K_TIMER_DEFINE(advertising_timer, ble_adv_timer_handler, NULL);

#if CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED

    /** Flag to check if we are connected to a device */
    static bool isConnected = false;

    /** Function for when the timer is done*/
    void ble_conn_timeout_timer_handler(struct k_timer *timer_id) { 
        isConnected = false; 
        LOG_WRN("Connection timed out (%ds)", CONFIG_BLE_CONN_TIMEOUT_SEC);
    }
    /** Timer for the advertising duration */
    K_TIMER_DEFINE(conn_timeout_timer, ble_conn_timeout_timer_handler, NULL);

    /** Pointer to the sleep time value */
    static uint16_t *sleep_time;

#endif

/** Service data array for the broadcast*/
static uint8_t service_data[22] = {0};

/** MAC address object for the advertising*/
static bt_addr_le_t addr;

/** Data to advertise for the device*/
static const struct bt_data adv_data[] = {
    #if CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED
        BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
        BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(SLEEP_TIME_SERVICE_UUID)),
    #endif
    BT_DATA(BT_DATA_SVC_DATA16, service_data, sizeof(service_data)),
};

/** Parameters for the broadcaster advertising set*/
static const struct bt_le_adv_param adv_param = {
        #if CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED
		    .options = (BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_USE_NAME | BT_LE_ADV_OPT_USE_IDENTITY | BT_LE_ADV_OPT_CONNECTABLE),
        #else
            .options = (BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_USE_NAME | BT_LE_ADV_OPT_USE_IDENTITY),
        #endif
		.interval_min = ((uint16_t)(CONFIG_BLE_MIN_ADV_INTERVAL_MS) / 0.625f),
		.interval_max = ((uint16_t)(CONFIG_BLE_MAX_ADV_INTERVAL_MS) / 0.625f),
		.secondary_max_skip = 0U,
		.peer = NULL,
};

static struct bt_le_ext_adv *ext_adv;

/**
 * @brief quickly encode a pair of float values into the service data
 * 
 * @param pos position in the service data array
 * @param id id of the value
 * @param val value to encode
 * 
 * @return int 0 if no error, error code otherwise
*/
static int ble_encode_pair(uint8_t pos, uint8_t id, float *val) {
    uint8_t whole, decimal;

    LOG_IF_ERR(floatSeparator(val, &whole, &decimal), "Unable to separate float");

    /* Encode sign in decimal value*/
    if(*val < 0) {
        whole = whole * -1;
        decimal += 100;
    }

    service_data[pos] = id;
    service_data[pos + 1] = whole;
    service_data[pos + 2] = decimal;

    return 0;
}

#if CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED

    /**
     * @brief Handle the connection event
     * 
     * @param conn Pointer to the connection object
     * @param err Error code
    */
    static void connected(struct bt_conn *conn, uint8_t err) {
        if (err) {
            LOG_ERR("Connection failed (err %u)", err);
            return;
        }

        char addr[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        LOG_INF("Connected %s",addr);

        isConnected = true;
        k_timer_start(&conn_timeout_timer, K_SECONDS(CONFIG_BLE_CONN_TIMEOUT_SEC), K_NO_WAIT);
    }

    /**
     * @brief Handle the disconnection event
     * 
     * @param conn Pointer to the connection object
     * @param reason Reason for the disconnection
    */
    static void disconnected(struct bt_conn *conn, uint8_t reason) {
        LOG_INF("Disconnected (reason %u)", reason);

        isConnected = false;
        k_timer_stop(&conn_timeout_timer);
        k_timer_stop(&advertising_timer);
        adv_time_done = true;
    }

    /* Define the connections events callbacks*/
    BT_CONN_CB_DEFINE(conn_callbacks) = {
        .connected = connected,
        .disconnected = disconnected,
    };

    /**
     * @brief Handle the sleep time write
     * 
     * @param conn Pointer to the connection object
     * @param attr Pointer to the attribute object
     * @param buf Pointer to the buffer
     * @param len Length of the buffer
     * @param offset Offset of the buffer
     * @param flags Flags
     * 
     * @return ssize_t Length of the buffer
    */
    ssize_t write_sleep_time(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, 
                    uint16_t len, uint16_t offset, uint8_t flags) 
    {

        if (offset + len > sizeof(sleep_time)) {
            LOG_ERR("Invalid offset or len for write_sleep_time");
            return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
        }

        // Cast the buffer pointer to uint16_t pointer
        const uint16_t *uint16_ptr = (const uint16_t *)buf;

        // Dereference the pointer to get the uint16_t value
        uint16_t parsed_value = *uint16_ptr;
        
        LOG_INF("Received new sleep time value: %d", parsed_value);

        // Check if the value is between 1 and the maximum sleep time
        if(parsed_value < 1) {
            LOG_WRN("Received sleep time value is too low: %d (seting to 1)", parsed_value);
            parsed_value = 1;
        }
        else if(parsed_value > CONFIG_SENSOR_SLEEP_DURATION_MAX_SEC) {
            LOG_WRN("Received sleep time value is too high: %d (seting to %d)", parsed_value, CONFIG_SENSOR_SLEEP_DURATION_MAX_SEC);
            parsed_value = CONFIG_SENSOR_SLEEP_DURATION_MAX_SEC;
        }

        // Update the sleep time value
        if(sleep_time == NULL) {
            LOG_ERR("Sleep time pointer is NULL");
            return len;
        }

        *sleep_time = parsed_value;

        k_timer_start(&conn_timeout_timer, K_SECONDS(CONFIG_BLE_CONN_TIMEOUT_SEC), K_NO_WAIT);

        return len;
    }

    /**
     * @brief Handle the sleep time read
     * 
     * @param conn Pointer to the connection object
     * @param attr Pointer to the attribute object
     * @param buf Pointer to the buffer
     * @param len Length of the buffer
     * @param offset Offset of the buffer
     * 
     * @return ssize_t Length of the buffer
    */
    ssize_t read_sleep_time(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                    uint16_t len, uint16_t offset) 
    {
        LOG_INF("Sending sleep time value: %d", *sleep_time);

        k_timer_start(&conn_timeout_timer, K_SECONDS(CONFIG_BLE_CONN_TIMEOUT_SEC), K_NO_WAIT);

        return bt_gatt_attr_read(conn, attr, buf, len, offset, sleep_time, sizeof(*sleep_time));
    }

    BT_GATT_SERVICE_DEFINE(sleep_time_service,
        BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_16(SLEEP_TIME_SERVICE_UUID)),
        BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(SLEEP_TIME_CHARACTERISTIC_UUID), BT_GATT_CHRC_WRITE | BT_GATT_CHRC_READ,
            BT_GATT_PERM_WRITE | BT_GATT_PERM_READ, read_sleep_time, write_sleep_time, &sleep_time),
    );

#endif

/**
 * @brief Start the advertising
*/
static void ble_start_adv(int err) {
    if (err) {
        LOG_ERR("Bluetooth failed to enable (err %d)", err);
        return;
    }

    /* Start advertising */
    LOG_INF("Starting advertising");

    LOG_IF_ERR(bt_set_name(DEVICE_NAME), "Unable to set broadcaster device name");

    LOG_IF_ERR(bt_le_ext_adv_create(&adv_param, NULL, &ext_adv), "Advertising failed to create");

    LOG_IF_ERR(bt_le_ext_adv_set_data(ext_adv, adv_data, ARRAY_SIZE(adv_data), NULL, 0), "Advertising failed to set data");

    counter++; // Increment the advertising counter

    LOG_IF_ERR(bt_le_ext_adv_start(ext_adv, BT_LE_EXT_ADV_START_DEFAULT), "Advertising faild to start");

    LOG_INF("Advertising started");
}

/** Callback handler for smp dfu */
struct mgmt_callback smp_mgmt_cb;

/**
 * @brief Handle the smp events 
*/
int32_t smp_mgmt_cb_handler(uint32_t event, int32_t rc, bool *abort_more, void *data,
                    size_t data_size)
{
    k_timer_start(&conn_timeout_timer, K_SECONDS(CONFIG_BLE_CONN_TIMEOUT_SEC), K_NO_WAIT);
    LOG_INF("SMP event: %d", event);
    return MGMT_ERR_EOK;
}

int ble_init(uint16_t *sleep_time_ptr) {

    if(isInisialized) {
        LOG_WRN("BLE already initialized");
        return 0;
    }

    k_timer_init(&advertising_timer, ble_adv_timer_handler, NULL);
    #if CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED
        k_timer_init(&conn_timeout_timer, ble_conn_timeout_timer_handler, NULL);
    #endif

    /* Setting sleep time pointer */
    #if CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED // The sleep time pointer is only needed if the sleep modification is enabled
        if(sleep_time_ptr == NULL) {
            LOG_ERR("Sleep time pointer is NULL");
            return -1;
        }
    #endif

    sleep_time = sleep_time_ptr;

    LOG_INF("Initializing bluetooth");

    LOG_INF("Setting MAC address");
    LOG_IF_ERR(bt_addr_le_from_str(CONFIG_BLE_USER_DEFINED_MAC_ADDR, "random", &addr), "Unable to convert MAC address");
    int id = bt_id_create(&addr, NULL);
    if (id != 0) {
        LOG_ERR("Unable to set MAC address");
        return id;
    }

    /* Setting service UUID */
    service_data[0] = BROADCAST_SERVICE_UUID_1;
    service_data[1] = BROADCAST_SERVICE_UUID_2;

    isInisialized = true;
    LOG_INF("Bluetooth initialized");

    return 0;
}

int ble_stop_adv(void) {
    if (!isInisialized) {
        LOG_ERR("BLE not initialized");
        return -1;
    }

    LOG_INF("Stopping advertising");

    LOG_IF_ERR(bt_le_ext_adv_stop(ext_adv), "Unable to stop advertising set");

    LOG_INF("Advertising stopped");

    return 0;
}

int ble_encode_adv_data(sensors_data_t *sensors_data) {

    LOG_DBG("Encoding data");

    /* Setting counter */
    service_data[2] = 0;
    service_data[3] = counter;

    /* Setting data */
    LOG_IF_ERR(ble_encode_pair(4, TEMP_ID, &sensors_data->temp), "Unable to encode temperature");
    LOG_IF_ERR(ble_encode_pair(7, HUM_ID, &sensors_data->hum), "Unable to encode humidity");
    LOG_IF_ERR(ble_encode_pair(10, LUM_ID, &sensors_data->lum), "Unable to encode luminosity");
    LOG_IF_ERR(ble_encode_pair(13, GND_TEMP_ID, &sensors_data->gnd_temp), "Unable to encode ground temperature");
    LOG_IF_ERR(ble_encode_pair(16, GND_HUM_ID, &sensors_data->gnd_hum), "Unable to encode ground humidity");
    LOG_IF_ERR(ble_encode_pair(19, BAT_ID, &sensors_data->bat), "Unable to encode battery");

    LOG_DBG("Data encoded");

    return 0;
}

int ble_adv(void) {
    if (!isInisialized) {
        LOG_ERR("BLE not initialized");
        return -1;
    }

    bool isSMPEnabled = false;
    if(get_button1_state()){
        LOG_INF("Button pressed, starting smp");
        isSMPEnabled = true;
        LOG_IF_ERR(smp_bt_register(), "Unable to register smp");

        smp_mgmt_cb.callback = smp_mgmt_cb_handler;
        smp_mgmt_cb.event_id = (MGMT_EVT_OP_CMD_STATUS);
        mgmt_callback_register(&smp_mgmt_cb);

        led1_on();

        k_timer_start(&advertising_timer, K_SECONDS(CONFIG_BLE_DFU_ADV_DURATION_SEC), K_NO_WAIT);
    }

    /* Enable bluetooth */
    LOG_INF("Enabling bluetooth");
    LOG_IF_ERR(bt_enable(ble_start_adv), "Unable to enable bluetooth");

    // Start the advertising timer
    adv_time_done = false;
    k_timer_start(&advertising_timer, K_SECONDS(CONFIG_BLE_ADV_DURATION_SEC), K_NO_WAIT);

    while (!adv_time_done) {
            k_yield();
            k_sleep(K_MSEC(10));  // Sleep for a short duration while waiting
    }

    LOG_INF("Advertising time done");

    if(get_button1_state()) LOG_INF("Button pressed, waiting to stop advertising");
    while(get_button1_state()) {
        k_yield();
        k_sleep(K_MSEC(10));
    }

    #if CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED
        if(isConnected) LOG_INF("Waiting for disconnection");
        while(isConnected) {
            k_yield();
            k_sleep(K_MSEC(10));
        }
    #endif

    LOG_IF_ERR(ble_stop_adv(), "Unable to stop advertising");
    LOG_INF("Disabling bluetooth");
    LOG_IF_ERR(bt_disable(), "Unable to disable bluetooth");

    if(isSMPEnabled) {
        LOG_INF("Unregistering smp");
        LOG_IF_ERR(smp_bt_unregister(), "Unable to unregister smp");
        mgmt_callback_unregister(&smp_mgmt_cb);
    }

    k_timer_stop(&advertising_timer);
    adv_time_done = true;
    #if CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED
        k_timer_stop(&conn_timeout_timer);
    #endif

    led1_off();

    return 0;
}