/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ble.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

LOG_MODULE_REGISTER(BLE);

#define AUTH_EVT_PRIMARY_DISCOVERED			0x01
#define AUTH_EVT_WRITE_CHRC_DISCOVERED		0x02
#define AUTH_EVT_READ_DISCOVERED			0x04
#define AUTH_EVT_READ_CCC_DISCOVERED		0x08
#define AUTH_EVT_NOTIFICATIONS_ENABLED		0x10
#define AUTH_EVT_ATTR_DISCOVER_FAIL			0x20
#define	AUTH_EVT_DATA_WRITTEN				0x40
#define AUTH_EVT_RESP_RECEIVED				0x80

#define MAIN_SERVICE_UUID 					BT_UUID_DECLARE_16(0xfea0)
#define WRITE_CHRC_UUID						BT_UUID_DECLARE_16(0xfea1)
#define READ_CHRC_UUID						BT_UUID_DECLARE_16(0xfea2)

// Default addresses if address if none saved to device
bt_addr_le_t default_address_buf[] = {
	{
	.type = BT_ADDR_LE_RANDOM,
	.a.val={0x29, 0x74, 0x06, 0x02, 0x34, 0xdd}
	},
	{
	.type = BT_ADDR_LE_RANDOM,
	.a.val={0x8a, 0x72, 0x06, 0x02, 0x34, 0xdd}
	},
	{
	.type = BT_ADDR_LE_RANDOM,
	.a.val={0x85, 0x6e, 0x06, 0x02, 0x34, 0xdd}
	},	
};

// address buffer structure
struct addr_filter_buf{
	bt_addr_le_t buf[DEFAULT_ADDR_FILTER_LEN];
	int head;
	int size;
};

struct remote_device_attr_info{
	uint16_t write_chrc_value_handle;
	uint16_t read_chrc_attr_handle;
	uint16_t read_ccc_handle;
	uint16_t read_chrc_value_handle;
};

static void start_scan();
static void stop_scan();

static int discover_attributes();
static uint8_t attribute_discovered(struct bt_conn *conn, const struct bt_gatt_attr *attr, struct bt_gatt_discover_params *params);
static uint8_t read_chrc_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_read_params *params, const void *data, uint16_t length);
static void write_chrc_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_write_params *params);
static uint8_t notify_cb(struct bt_conn *conn, struct bt_gatt_subscribe_params *params, const void *data, uint16_t length);
static void subscribe_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_subscribe_params *params);

K_MSGQ_DEFINE(ble_msgq, sizeof(struct ble_msg), 5, 4);
K_EVENT_DEFINE(auth_evts);

extern struct k_event main_evts;
extern uint8_t device_password[8];

struct bt_conn *default_conn;
bt_addr_le_t *last_scanned_address = NULL;
struct addr_filter_buf addr_filter;
struct remote_device_attr_info attr_info = {0, 0, 0}; 
bool authentication_enabled = false;
// Transmit buffer
uint8_t tx_buf[6];
// Receive buffer
uint8_t rx_buf[22];

static struct bt_gatt_discover_params d_params = {
	.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE,
	.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE,
	.func = attribute_discovered,
	.type = BT_GATT_DISCOVER_PRIMARY,
};

static struct bt_gatt_read_params read_params = {
	.func = read_chrc_cb,
};

static struct bt_gatt_write_params write_params = {
	.func = write_chrc_cb,
	.data = tx_buf,
	.length = sizeof(tx_buf)
};

static struct bt_gatt_subscribe_params sub_params = {
	.subscribe = subscribe_cb,
	.notify = notify_cb
};


static int address_in_filter(const bt_addr_le_t *addr)
{
	for(int i=0; i<addr_filter.size; ++i){
		if(bt_addr_le_cmp(addr, &addr_filter.buf[i]) == 0){
			return i;
		}
	}

	return -1;
}

// Scanned device found callback
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
    int res = 0;
	char addr_str[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

    // check if address is in authorised filter
    res = address_in_filter(addr);
    if(res < 0){
        return;
    }

	if(bt_addr_le_cmp(addr, last_scanned_address) != 0){
		LOG_DBG("Device found: %s", addr_str);
	}

	last_scanned_address = &addr_filter.buf[res];

	if(authentication_enabled){
		stop_scan();

		res = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &default_conn);
		if(res){
			LOG_ERR("Create connection fail (err %d)", res);
			start_scan();
		}
		return;
	}

	k_event_set(&main_evts, MAIN_EVT_BLE_DEVICE_FOUND);
}

static void start_scan(void)
{
	int err;
	err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
	if (err) {
		LOG_ERR("Scanning failed to start (err %d)\n", err);
		return;
	}

	LOG_INF("Scanning started");
}

static void stop_scan()
{
	int err = 0;
	err = bt_le_scan_stop();
	if(err){
		LOG_ERR("Scan stop fail (err %d)", err);
		return;
	}

	LOG_DBG("Scanning stopped");
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if(err){
		LOG_ERR("BLE connect fail (err %d)", err);
		return;
	}

	LOG_INF("Remote device connected");

	k_event_set(&main_evts, MAIN_EVT_BLE_DEVICE_CONNECTED);

	d_params.type = BT_GATT_DISCOVER_PRIMARY;
	d_params.uuid = MAIN_SERVICE_UUID;
	int res = bt_gatt_discover(conn, &d_params);
	if(res){
		LOG_ERR("Discover primary service fail(err %d)", res);
		k_event_set(&auth_evts, AUTH_EVT_ATTR_DISCOVER_FAIL);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("BLE disconnected (reason %d)", reason);
	bt_conn_unref(conn);
	default_conn = NULL;
	start_scan();
}

static uint8_t attribute_discovered(struct bt_conn *conn, const struct bt_gatt_attr *attr, struct bt_gatt_discover_params *params)
{
	if(params->type == BT_GATT_DISCOVER_PRIMARY){
		LOG_DBG("Primary service discovered");
		k_event_set(&auth_evts, AUTH_EVT_PRIMARY_DISCOVERED);
	}
	else if(params->type == BT_GATT_DISCOVER_CHARACTERISTIC){
		if(bt_uuid_cmp(params->uuid, WRITE_CHRC_UUID) == 0){
			LOG_DBG("Write chrc discovered");
			struct bt_gatt_chrc *w_chrc = (struct bt_gatt_chrc *)attr->user_data;
			attr_info.write_chrc_value_handle = w_chrc->value_handle;
			k_event_set(&auth_evts, AUTH_EVT_WRITE_CHRC_DISCOVERED);
		}
		else if(bt_uuid_cmp(params->uuid, READ_CHRC_UUID) == 0){
			LOG_DBG("read chrc discovered");
			struct bt_gatt_chrc *r_chrc = (struct bt_gatt_chrc *)attr->user_data;
			attr_info.read_chrc_value_handle = r_chrc->value_handle;
			attr_info.read_chrc_attr_handle = bt_gatt_attr_get_handle(attr);
			k_event_set(&auth_evts, AUTH_EVT_READ_DISCOVERED);
		}
	}
	else if(params->type == BT_GATT_DISCOVER_DESCRIPTOR){
		if(bt_uuid_cmp(params->uuid, BT_UUID_GATT_CCC) == 0){
			attr_info.read_ccc_handle = bt_gatt_attr_get_handle(attr);
			k_event_set(&auth_evts, AUTH_EVT_READ_CCC_DISCOVERED);
		}
	}

	return BT_GATT_ITER_STOP;
}

static uint8_t read_chrc_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_read_params *params, const void *data, uint16_t length)
{
	if(err){
		LOG_ERR("Read resp fail(err %d)", err);
		return BT_GATT_ITER_STOP;
	}

	memcpy(rx_buf, data, length);
	k_event_set(&auth_evts, AUTH_EVT_RESP_RECEIVED);
	return BT_GATT_ITER_STOP;
}

static void write_chrc_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_write_params *params)
{
	if(err){
		LOG_ERR("Write data fail(err %d)", err);
		return;
	}

	k_event_set(&auth_evts, AUTH_EVT_DATA_WRITTEN);
}

static uint8_t notify_cb(struct bt_conn *conn, struct bt_gatt_subscribe_params *params, const void *data, uint16_t length)
{
	if(data == NULL){
		LOG_WRN("Notifications disabled");
		return BT_GATT_ITER_STOP;
	}

	memcpy(rx_buf, data, length);
	k_event_set(&auth_evts, AUTH_EVT_RESP_RECEIVED);

	return BT_GATT_ITER_CONTINUE;
}

static void subscribe_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_subscribe_params *params)
{
	if(err){
		LOG_ERR("Notifications enable fail(err %d)", err);
		return;
	}

	if(params->value == BT_GATT_CCC_NOTIFY){
		LOG_DBG("Notifications enabled");
		k_event_set(&auth_evts, AUTH_EVT_NOTIFICATIONS_ENABLED);
	}
}


static int discover_attributes()
{
	int ret = 0;
	uint32_t evts = 0;
	evts = k_event_wait(&auth_evts, AUTH_EVT_PRIMARY_DISCOVERED, false, K_MSEC(5000));
	if(!evts){
		LOG_ERR("Discover primary timeout");
		return -ETIMEDOUT;
	}

	d_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
	d_params.uuid = WRITE_CHRC_UUID;
	ret = bt_gatt_discover(default_conn, &d_params);
	if(ret){
		LOG_ERR("Discover write fail(err %d)", ret);
		return ret;
	}

	evts = k_event_wait(&auth_evts, AUTH_EVT_WRITE_CHRC_DISCOVERED, true, K_MSEC(5000));
	if(!evts){
		LOG_ERR("Discover write chrc timeout");
		return -ETIMEDOUT;
	}

	d_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
	d_params.uuid = READ_CHRC_UUID;
	ret = bt_gatt_discover(default_conn, &d_params);
	if(ret){
		LOG_ERR("Discover read fail(err %d)", ret);
		return ret;
	}

	evts = k_event_wait(&auth_evts, AUTH_EVT_READ_DISCOVERED, true, K_MSEC(5000));
	if(!evts){
		LOG_ERR("Discover read chrc timeout");
		return -ETIMEDOUT;
	}

	d_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
	d_params.uuid = BT_UUID_GATT_CCC;
	d_params.start_handle = attr_info.read_chrc_attr_handle;
	ret = bt_gatt_discover(default_conn, &d_params);
	if(ret){
		LOG_ERR("Discover read CCC fail(err %d)", ret);
		return ret;
	}

	evts = k_event_wait(&auth_evts, AUTH_EVT_READ_CCC_DISCOVERED, true, K_MSEC(5000));
	if(!evts){
		LOG_ERR("Discover read CCC timeout");
		return -ETIMEDOUT;
	}

	sub_params.ccc_handle = attr_info.read_ccc_handle;
	sub_params.value_handle = attr_info.read_chrc_value_handle;
	sub_params.value = BT_GATT_CHRC_NOTIFY;
	ret = bt_gatt_subscribe(default_conn, &sub_params);
	if(ret){
		LOG_ERR("Enable notifications fail (err %d)", ret);
		return ret;
	}

	evts = k_event_wait(&auth_evts, AUTH_EVT_NOTIFICATIONS_ENABLED, true, K_MSEC(5000));
	if(!evts){
		LOG_ERR("Enable notifications timeout");
		return -ETIMEDOUT;
	}

	return 0;
}

static int write_auth_data(uint8_t *random_vars)
{
	int ret = 0;

	tx_buf[0] = 0x13;
	tx_buf[1] = 0x01;
	memcpy(&tx_buf[2], random_vars, 4);

	write_params.data = tx_buf;
	write_params.length = sizeof(tx_buf);
	write_params.handle = attr_info.write_chrc_value_handle;
	ret = bt_gatt_write(default_conn, &write_params);
	if(ret){
		LOG_ERR("GATT write fail (err %d)", ret);
		return ret;
	}

	uint32_t evt = k_event_wait(&auth_evts, AUTH_EVT_DATA_WRITTEN, true, K_SECONDS(5));
	if(!evt){
		LOG_ERR("Gatt write timeout");
		return -ETIMEDOUT;
	}

	return 0;
}

static int wait_auth_resp()
{
	uint32_t evt = k_event_wait(&auth_evts, AUTH_EVT_RESP_RECEIVED, true, K_SECONDS(5));
	if(!evt){
		LOG_WRN("Wait resp timeout, attempting read");

		// No notification received, read data
		read_params.single.handle = attr_info.read_chrc_value_handle;
		read_params.single.offset = 0;
		int ret = bt_gatt_read(default_conn, &read_params);
		if(ret){
			LOG_ERR("GATT read fail (err %d)", ret);
			return ret;
		}

		evt = k_event_wait(&auth_evts, AUTH_EVT_RESP_RECEIVED, true, K_SECONDS(5));
		if(!evt){
			LOG_ERR("Read resp timeout");
			return -ETIMEDOUT;
		}
	}

	if(rx_buf[0] != 0x13 || rx_buf[1] != 0x01){
		LOG_ERR("Invalid resp");
		return -EINVAL;
	}
	
	return 0;
}

static int check_resp_data(uint8_t *random, const bt_addr_le_t *addr)
{
	uint8_t raw_data[] = {
		addr->a.val[0],
		addr->a.val[1],
		addr->a.val[2],
		addr->a.val[3],
		addr->a.val[4],
		addr->a.val[5],
		0xa9,
		0xb1,
		random[0],
		random[1],
		random[2],
		random[3],
		device_password[0],
		device_password[1],
		device_password[2],
		device_password[3],
		device_password[4],
		device_password[5],
		device_password[6],
		device_password[7]
	};

	uint8_t md5_checksum_generated[16];
	uint8_t md5_checksum_received[16];

	LOG_DBG("Received checksum");
	for(int i=0; i<sizeof(md5_checksum_received); ++i){
		md5_checksum_received[i] = rx_buf[i];
		printk("%02x", md5_checksum_received[i]);
	}
	LOG_DBG("\n");
	
	int ret = 0;
	ret = generate_md5_checksum(raw_data, sizeof(raw_data), md5_checksum_generated);
	if(ret){
		LOG_ERR("Generate md5 fail(err %d)", ret);
		return ret;
	}

	LOG_DBG("Generated checksum");
	for(int i=0; i<sizeof(md5_checksum_generated); ++i){
		printk("%02x", md5_checksum_generated[i]);
		k_sleep(K_MSEC(100));
	}
	LOG_DBG("\n");

	if(memcmp(md5_checksum_generated, md5_checksum_received, sizeof(md5_checksum_generated)) != 0){
		LOG_ERR("Checksum not match");
		return -EINVAL;
	}

	return 0;
}

static void authenticate_remote_device()
{
	if(discover_attributes()){
		k_event_set(&main_evts, MAIN_EVT_BLE_DEVICE_AUTHENTICATION_FAIL);
		return;
	}
	
	uint8_t random_data[4];
	int res = 0;
	res = generate_random_numbers(sizeof(random_data), random_data);
	if(res){
		LOG_WRN("RNG fail, using default chars");
		memset(random_data, 0xff, sizeof(random_data));
	}

	res = write_auth_data(random_data);
	if(res){
		k_event_set(&main_evts, MAIN_EVT_BLE_DEVICE_AUTHENTICATION_FAIL);
		return;
	}

	res = wait_auth_resp();
	if(res){
		k_event_set(&main_evts, MAIN_EVT_BLE_DEVICE_AUTHENTICATION_FAIL);
		return;
	}

	res = check_resp_data(random_data, bt_conn_get_dst(default_conn));
	if(res){
		LOG_ERR("Authentication fail");
		k_event_set(&main_evts, MAIN_EVT_BLE_DEVICE_AUTHENTICATION_FAIL);
		return;
	}

	k_event_set(&main_evts, MAIN_EVT_BLE_DEVICE_AUTHENTICATED);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static int init_addr_filter()
{
	int ret = 0;
	bool success = true;
	for(int i=0; i<DEFAULT_ADDR_FILTER_LEN; ++i){
		bt_addr_le_t temp_addr = {
			.type = BT_ADDR_LE_RANDOM,
		};

		ret = read_nvs_data(i, temp_addr.a.val, sizeof(temp_addr.a.val));
		if(ret){
			if(i != 0){
				// At least one address found
				break;
			}
			LOG_WRN("Init address filter fail(err %d), using test addresses", ret);
			success = false;
			break;
		}

		ble_add_addr_to_filter(&temp_addr);
	}

	if(!success){
		memset(&addr_filter, 0, sizeof(addr_filter));
		ble_add_addr_to_filter(&default_address_buf[0]);
		ble_add_addr_to_filter(&default_address_buf[1]);
		ble_add_addr_to_filter(&default_address_buf[2]);
	}

	return 0;
}

void ble_thread_main(void)
{
	LOG_DBG("Start ble thread");
	int res = 0;

	res = init_addr_filter();
	if(res){
		return;
	}

	if(init_mbedtls_encryption()){
		return;
	}

	LOG_DBG("Enable BLE");
	res = bt_enable(NULL);
	if(res){
		LOG_ERR("BLE enable fail (err %d)", res);
		return;
	}

	LOG_DBG("Start scan");
	start_scan();

	struct ble_msg msg;
	while(1){
		k_msgq_get(&ble_msgq, &msg, K_FOREVER);
		if(msg.type == BLE_MSG_TYPE_ENABLE_AUTHENTICATION){
			LOG_INF("Enable authentication");
			authentication_enabled = true;
		}
		else if(msg.type == BLE_MSG_TYPE_START_AUTHENTICATION){
			LOG_INF("Authenticate remote device");
			authenticate_remote_device();
		}
		else if(msg.type == BLE_MSG_TYPE_STOP_AUTHENTICATION){
			LOG_INF("Stop athentication");
			if(default_conn != NULL){
				int res = bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
				if(res){
					LOG_ERR("BLE disconnect device error %d", res);
				}
			}
			authentication_enabled = false;
		}
	}
}

int ble_add_addr_to_filter(bt_addr_le_t *addr)
{
	if(addr_filter.head >= DEFAULT_ADDR_FILTER_LEN){
		addr_filter.head = 0;
	}

	bt_addr_le_copy(&addr_filter.buf[addr_filter.head], addr);
	addr_filter.head++;
	addr_filter.size++;

	if(addr_filter.size >= DEFAULT_ADDR_FILTER_LEN){
		addr_filter.size = DEFAULT_ADDR_FILTER_LEN;
	}

	char addr_str[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	LOG_DBG("Add address to filter %s", addr_str);

	return 0;
}

// create and start BLE thread
K_THREAD_DEFINE(ble_thread, 1024, ble_thread_main, NULL, NULL, NULL, 1, K_ESSENTIAL, 0);
