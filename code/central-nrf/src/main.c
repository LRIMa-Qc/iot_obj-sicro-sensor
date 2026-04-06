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
#define COMPANY_ID_LE 0x0059
#define PAYLOAD_SIZE 25
#define BIT_TEMP 0x01
#define BIT_HUM 0x02
#define BIT_LUM 0x04
#define BIT_CO2 0x08
#define BIT_GND_TEMP 0x10
#define BIT_GND_HUM 0x20
#define BIT_BAT 0x40
//==================================================================================================

//==================================================================================================
// UART defines
#define NAME_LEN 30
#define DATA_LEN 30
#define MAX_MYRIO_DATA_LEN 80

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
	valueDict[6] = 'C'; // CO2
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
 * Ex : I=01P=254T=25.40H=50.00C=42.50L=100.00S=000.00V=000.00B=10.20
 * 
 * @param array
 * @param length
 * @param result
 * @param resultSize
 * @param startIndex
*/
int convertToMyrio(uint8_t *array, size_t length, char *result, size_t resultSize, uint8_t startIndex)
{
	if (length < PAYLOAD_SIZE) {
		return 1;
	}

	uint8_t currentIndex = startIndex;
	uint16_t nodeId = (uint16_t)array[2] | ((uint16_t)array[3] << 8);
	uint8_t sequence = array[4];
	uint8_t presentMask = array[21];

	int bytesWritten = snprintf(result + currentIndex, resultSize - currentIndex, "I=%02uP=%u", nodeId, sequence);
	if (bytesWritten < 0 || bytesWritten >= resultSize - currentIndex) {
		return 1;
	}
	currentIndex += bytesWritten;

	if (presentMask & BIT_TEMP) {
		int16_t temp = (int16_t)((uint16_t)array[5] | ((uint16_t)array[6] << 8));
		bytesWritten = snprintf(result + currentIndex, resultSize - currentIndex, "T=%d.%02d",
				temp / 100, abs(temp % 100));
		if (bytesWritten < 0 || bytesWritten >= resultSize - currentIndex) {
			return 1;
		}
		currentIndex += bytesWritten;
	}

	if (presentMask & BIT_HUM) {
		uint16_t hum = (uint16_t)array[7] | ((uint16_t)array[8] << 8);
		bytesWritten = snprintf(result + currentIndex, resultSize - currentIndex, "H=%u.%02u",
				hum / 100, hum % 100);
		if (bytesWritten < 0 || bytesWritten >= resultSize - currentIndex) {
			return 1;
		}
		currentIndex += bytesWritten;
	}

	if (presentMask & BIT_LUM) {
		uint32_t lum = (uint32_t)array[9] | ((uint32_t)array[10] << 8) |
			      ((uint32_t)array[11] << 16) | ((uint32_t)array[12] << 24);
		bytesWritten = snprintf(result + currentIndex, resultSize - currentIndex, "L=%lu",
				(unsigned long)lum);
		if (bytesWritten < 0 || bytesWritten >= resultSize - currentIndex) {
			return 1;
		}
		currentIndex += bytesWritten;
	}

	if (presentMask & BIT_CO2) {
		uint16_t co2 = (uint16_t)array[13] | ((uint16_t)array[14] << 8);
		bytesWritten = snprintf(result + currentIndex, resultSize - currentIndex, "C=%u", co2);
		if (bytesWritten < 0 || bytesWritten >= resultSize - currentIndex) {
			return 1;
		}
		currentIndex += bytesWritten;
	}

	if (presentMask & BIT_GND_TEMP) {
		int16_t gtemp = (int16_t)((uint16_t)array[15] | ((uint16_t)array[16] << 8));
		bytesWritten = snprintf(result + currentIndex, resultSize - currentIndex, "S=%d.%02d",
				gtemp / 100, abs(gtemp % 100));
		if (bytesWritten < 0 || bytesWritten >= resultSize - currentIndex) {
			return 1;
		}
		currentIndex += bytesWritten;
	}

	if (presentMask & BIT_GND_HUM) {
		uint16_t ghum = (uint16_t)array[17] | ((uint16_t)array[18] << 8);
		bytesWritten = snprintf(result + currentIndex, resultSize - currentIndex, "V=%u.%02u",
				ghum / 100, ghum % 100);
		if (bytesWritten < 0 || bytesWritten >= resultSize - currentIndex) {
			return 1;
		}
		currentIndex += bytesWritten;
	}

	if (presentMask & BIT_BAT) {
		uint16_t bat = (uint16_t)array[19] | ((uint16_t)array[20] << 8);
		bytesWritten = snprintf(result + currentIndex, resultSize - currentIndex, "B=%u.%02u",
				bat / 100, bat % 100);
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
static bool isNewAdvertisingPacket(char *name, char *addr, struct service_data *srv_data)
{
	if (srv_data->len < PAYLOAD_SIZE) {
		return false;
	}

	uint16_t companyId = (uint16_t)srv_data->data[0] | ((uint16_t)srv_data->data[1] << 8);
	if (companyId != COMPANY_ID_LE) {
		return false;
	}

	uint16_t nodeId = (uint16_t)srv_data->data[2] | ((uint16_t)srv_data->data[3] << 8);
	uint8_t packetNumber = srv_data->data[4];

	struct remote_device_t newDevice = {
		.name = name,
		.addr = addr,
		.packetNumber = packetNumber,
	};

	struct remote_device_t device = remoteDevices[nodeId % 100];
	if (strcmp(device.name, newDevice.name) == 0 && strcmp(device.addr, newDevice.addr) == 0) {
		if (device.packetNumber == newDevice.packetNumber) {
			return false;
		}

		device.packetNumber = newDevice.packetNumber;
		remoteDevices[nodeId % 100] = device;
		return true;
	}

	remoteDevices[nodeId % 100] = newDevice;
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

	myrioData[0] = '\0';
	LOG_IF_ERR(convertToMyrio(srv_data->data, srv_data->len, myrioData, sizeof(myrioData), 0), "Error converting data to string\n"); // Convert data to string

	printk("Myrio data: %s\n", myrioData);
	uart_tx(uart1, myrioData, strlen(myrioData), SYS_FOREVER_MS); // Send data via UART1

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

	// LOG_IF_ERR(uart_rx_enable(uart1 ,rx_buf,sizeof rx_buf,RECEIVE_TIMEOUT), "UART RX failed\n"); // Enable UART RX
	// LOG_INF("UART RX enabled\n");

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
