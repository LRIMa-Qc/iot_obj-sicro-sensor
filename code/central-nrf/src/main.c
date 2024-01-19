/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>

LOG_MODULE_REGISTER(MAIN, CONFIG_MAIN_LOG_LEVEL);

//==================================================================================================
// Macros for logging
#define STRING(x) #x
#define TO_STRING(x) STRING(x)
#define LOCATION __FILE__ ":" TO_STRING(__LINE__)
#define LOG_IF_ERR(expr, msg)                                   			\
    {                                                        								 	 \
        int ret = (expr);                                    						 	   \
        if(ret) {                                            								   	  \
            LOG_ERR("Error %d: " msg " in " LOCATION, ret);            \
    	}																							   \
	}
//==================================================================================================

//==================================================================================================
// Bluetooth defines
#define SERVICE_UUID_1 0xAB
#define SERVICE_UUID_2 0xCD
//==================================================================================================

//==================================================================================================
// UART defines
#define NAME_LEN 30
#define DATA_LEN 30
#define MAX_MYRIO_DATA_LEN 60

#define MAX_DATA_LEN 255

#define TX_BUFF_SIZE 40
#define RX_BUFF_SIZE 10
#define RECEIVE_TIMEOUT 100
//==================================================================================================

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

const struct device *uart1= DEVICE_DT_GET(DT_NODELABEL(uart1));

static uint8_t tx_buf[TX_BUFF_SIZE] = {"Init\n\r"};

static uint8_t rx_buf[RX_BUFF_SIZE] = {0};

typedef struct service_data {
		uint8_t len;
		uint8_t data[DATA_LEN];
};

typedef struct remote_device_t {
		char name[NAME_LEN];
		char addr[BT_ADDR_LE_STR_LEN];
		uint8_t packetNumber;
};

// List of remote devices that have been detected
static struct remote_device_t remoteDevices[100] = {0};

bool isLedOn = false;

static char valueDict[MAX_DATA_LEN] = {0};

static void initValueDict() {
	valueDict[0] = 'P'; // Packet number
	valueDict[1] = 'T'; // Temperature
	valueDict[2] = 'H'; // Humidity
	valueDict[3] = 'L'; // Luminoisty
	valueDict[4] = 'S'; // Humidity GND
	valueDict[5] = 'V'; // Temperature GND
	valueDict[254] = 'B'; // Battery

}

/**
 * @brief Convert an array of bytes to a string of hex values separated by hyphens
 * 
 * @param array
 * @param length
 * @param result
 * @param resultSize
 * @return int 0 if successful, 1 if error
 */
int convertArray(uint8_t* array, size_t length, char* result, size_t resultSize)
{
    size_t currentIndex = 0;
    for (size_t i = 0; i < length; i++) {
        int bytesWritten = snprintf(result + currentIndex, resultSize - currentIndex, "%02x-", array[i]);
        if (bytesWritten < 0 || bytesWritten >= resultSize - currentIndex) {
            return 1;
        }
        currentIndex += bytesWritten;
    }

    result[currentIndex - 1] = '\0'; // Replace the last hyphen with a null terminator
	return 0;
}

/**
 * @brief Convert the data received from the sensor to a string for the Myrio
 * Ex : I=01P=254T=25.40H=50.00L=100.00S=000.00V=000.00B=10.20
 * 
 * @param array
 * @param length
 * @param result
 * @param resultSize
 * @param startIndex
*/
int convertToMyrio(uint8_t* array, size_t length, char* result, size_t resultSize, uint8_t startIndex)
{
	uint8_t currentIndex = startIndex;

	// Get the current packet number 
	char key = valueDict[array[2]];
	int whole = array[3];

	// Add the packet number to the result string
	int bytesWritten = snprintf(result + currentIndex, resultSize - currentIndex, "%c=%d", key, whole);
	if (bytesWritten < 0 || bytesWritten >= resultSize - currentIndex) {
		return 1;
	}
	currentIndex += bytesWritten;

	// Start looping from the 4th byte of the array
	for (size_t i = 4; i < length; i += 3) {
		
		// Get the key of the value (ex : T for temperature)
		key = valueDict[array[i]];

		// Check if the key is valid (ex : if the key is 0, it means that the value is invalid)
		if (key == 0) {
			LOG_WRN("Invalid key\n\r");
			return 1;
		}

		// Get the whole part of the value 
		whole = array[i + 1];

		// Get the decimal part of the value
		int decimal = array[i + 2];

		// Put the key and the value in the result string
		bytesWritten = snprintf(result + currentIndex, resultSize - currentIndex, "%c=%d.%02d", key, whole, decimal);
		if (bytesWritten < 0 || bytesWritten >= resultSize - currentIndex) {
			return 1;
		}
		currentIndex += bytesWritten;
	}
	return 0;
}

/**
 * @brief Check if the advertising packet is new
 * 
 * @param name
 * @param addr
 * @param srv_data
 * @return static bool
*/
static bool isNewAdvertisingPacket(char* name, char* addr, struct service_data* srv_data) {
	// Check if it has the right servie UUID
	if (srv_data->data[0] != SERVICE_UUID_1 || srv_data->data[1] != SERVICE_UUID_2) {
		return false;
	}

	// Get the packet number
	if(valueDict[srv_data->data[2]] != 'P') return false;
	uint8_t packetNumber = srv_data->data[3];

	// Create a new device
	struct remote_device_t newDevice = {
		.name = name,
		.addr = addr,
		.packetNumber = packetNumber,
	};

	// Get the device at the index of the two last char of the name (ex : 01, or 23)
	uint8_t index = atoi(name + strlen(name) - 2);
	struct remote_device_t device = remoteDevices[index];

	// Check if the device is the same as the last one
	if (strcmp(device.name, newDevice.name) == 0 && strcmp(device.addr, newDevice.addr) == 0) {
		// Check if the packet number is the same as the last one
		if (device.packetNumber == newDevice.packetNumber) return false;

		// Update the packet number of the device
		device.packetNumber = newDevice.packetNumber;
		remoteDevices[index] = device;

		return true;
	}

	// Update the device
	remoteDevices[index] = newDevice;
	return true;
}

/**
 * @brief Send value to computer
 * 
 * @param name
 * @param addr
 * @param srv_data
 * @return static void
*/
static void send_value(char* name, char* addr, struct service_data* srv_data)
{
	// Data sent via USB
    char data[DATA_LEN * 3];
    LOG_IF_ERR(convertArray(srv_data->data, srv_data->len, data, sizeof(data)), "Error converting data to string\n"); // Convert data to string

	printk("{%s,%s,%s}\n", name, addr, data); // Print name, address, and converted data


	// Data sent via UART
	char myrioData[MAX_MYRIO_DATA_LEN];

	myrioData[0] = 'I'; // Device ID
	myrioData[1] = '='; // Separator
	// Get the two last char of the name and set them as the device ID
	myrioData[2] = name[strlen(name) - 2];
	myrioData[3] = name[strlen(name) - 1];

	LOG_IF_ERR(convertToMyrio(srv_data->data, srv_data->len, myrioData, sizeof(myrioData), 4), "Error converting data to string\n"); // Convert data to string

	printk("Myrio data: %s\n", myrioData);
	uart_tx(uart1, myrioData, sizeof(myrioData), SYS_FOREVER_MS); // Send data via UART1

	// Toggle LED
	gpio_pin_set(led0.port, led0.pin, isLedOn); // Toggle LED
	isLedOn = !isLedOn;
}

/**
 * @brief Callback function for name
 * 
 * @param data
 * @param user_data
 * @return static bool
*/
static bool data_cb_name(struct bt_data *data, void *user_data)
{
	char *name = user_data;
	uint8_t len;

	switch (data->type) {
	case BT_DATA_NAME_SHORTENED:
	case BT_DATA_NAME_COMPLETE:
		len = MIN(data->data_len, NAME_LEN - 1);
		(void)memcpy(name, data->data, len);
		name[len] = '\0';
		return false;
	default:
		return true;
	}
}

/**
 * @brief Callback function for service data
 * 
 * @param data
 * @param user_data
 * @return static bool
*/
static bool data_cb_service(struct bt_data *data, void *user_data){
	struct service_data *svc_data = user_data;

	switch (data->type) { // Get service data
	case BT_DATA_SVC_DATA16:  // 16-bit UUID
		svc_data->len = MIN(data->data_len, DATA_LEN - 1);
		(void)memcpy(svc_data->data, data->data, svc_data->len);
		svc_data->data[svc_data->len] = '\0';
		return false;
	default:
		return true;
	}
}

/**
 * @brief Callback function for UART
 * 
 * @param dev
 * @param evt
 * @param user_data
*/
static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
	switch (evt->type) {

	case UART_RX_RDY:
		printk("Received data: %s\n\r", evt->data.rx.buf);
        // Check if the received data is a whole number bigger than 0 and smaller than the max size for a uint16_t
		if (atoi(evt->data.rx.buf) > 0 && atoi(evt->data.rx.buf) < UINT16_MAX) {
			LOG_INF("Received data: %s\n\r", evt->data.rx.buf);
			memcpy(tx_buf, evt->data.rx.buf, sizeof(tx_buf));
			uart_tx(dev, tx_buf, sizeof(tx_buf), SYS_FOREVER_MS);
			LOG_INF("Sent data: %s\n\r", tx_buf);
		}
		else {
			LOG_INF("Received data: %s\n\r", evt->data.rx.buf);
			memcpy(tx_buf, "Invalid input\n\r", sizeof(tx_buf));
			uart_tx(dev, tx_buf, sizeof(tx_buf), SYS_FOREVER_MS);
			LOG_WRN("Invalid input %s\n\r", evt->data.rx.buf);
		}
	break;
	case UART_RX_DISABLED:
        printk("RX_DISABLED\n\r");
		uart_rx_enable(dev ,rx_buf,sizeof rx_buf,RECEIVE_TIMEOUT);
        printk("UART RX\n\r");
		break;
		
	default:
		break;
	}
}

/**
 * @brief Callback function for received data
 * 
 * @param info
 * @param buf
 * @return static void
 * 
*/
static void scan_recv(const struct bt_le_scan_recv_info *info,
		      struct net_buf_simple *buf)
{
	char le_addr[BT_ADDR_LE_STR_LEN];
	char name[NAME_LEN];
	struct net_buf_simple ad;
	struct service_data svc_data = {
		.len = 0,
		.data = {0},
	};

	(void)memset(name, 0, sizeof(name)); // Clear name

	(void)memcpy(&ad, buf, sizeof(ad)); // Copy buffer

	bt_data_parse(&ad, data_cb_name, name); // Get name

	if (name[0] == '\0') return; // If no name, ignore

	bt_data_parse(buf, data_cb_service, &svc_data); // Get service data

	if (svc_data.len < 1) return; // If no service data, ignore)
	
	bt_addr_to_str(&info->addr->a, le_addr, sizeof(le_addr)); // Get address

	if(CONFIG_BLE_ENABLE_MAC_ADDR_FILTER &&  strncmp(le_addr, CONFIG_BLE_USER_DEFINED_MAC_ADDR_FILTER, strlen(CONFIG_BLE_USER_DEFINED_MAC_ADDR_FILTER)) != 0) return; // If not the correct address, ignore

	if (!isNewAdvertisingPacket(name, le_addr, &svc_data)) return; // If not a new advertising packet, ignore

	LOG_INF("Received data from %s (%s)\n", le_addr, name);

	send_value(name, le_addr, &svc_data); // Send data to computer
}

static struct bt_le_scan_cb scan_callbacks = { 
	.recv = scan_recv,
};

void main(void)
{
	initValueDict();

	LOG_IF_ERR(!device_is_ready(uart1), "UART device not ready\n"); // Check if UART device is ready
	LOG_INF("Device UART ready\n");

	LOG_IF_ERR(!device_is_ready(led0.port), "Led0 device not ready\n"); // Check if GPIO device is ready
	LOG_INF("Device Led0 ready\n");

	LOG_IF_ERR(gpio_pin_configure_dt(&led0, GPIO_OUTPUT_ACTIVE), "LED0 configuration failed\n"); // Configure LED0
	LOG_INF("LED0 configured\n");

	LOG_IF_ERR(uart_callback_set(uart1, uart_cb, NULL), "UART callback not set\n"); // Set UART callback
	LOG_INF("UART callback set\n");

	LOG_IF_ERR(uart_rx_enable(uart1 ,rx_buf,sizeof rx_buf,RECEIVE_TIMEOUT), "UART RX failed\n"); // Enable UART RX
	LOG_INF("UART RX enabled\n");

	LOG_IF_ERR(bt_enable(NULL), "Bluetooth init failed\n"); // Initialize Bluetooth
	
	bt_le_scan_cb_register(&scan_callbacks); // Register scan callback

	LOG_INF("Bluetooth initialized\n");

	struct bt_le_scan_param scan_param = { // Set scan parameters
		.type       = BT_LE_SCAN_TYPE_PASSIVE,
		.options    = BT_LE_SCAN_OPT_CODED,
		.interval   = 0x0200,
		.window     = BT_GAP_SCAN_FAST_WINDOW,
	};

	LOG_IF_ERR(bt_le_scan_start(&scan_param, NULL), "Scanning failed to start\n"); // Start scanning
	LOG_INF("Scanning successfully started\n"); 
}
