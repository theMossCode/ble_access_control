/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <errno.h>
#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(BLE);

static void start_scan();
static void stop_scan();

// create BLE event kernel object
K_EVENT_DEFINE(ble_events);

// Default connection reference
static struct bt_conn *default_conn;
// Address of last scanned device
bt_addr_le_t *nearby = NULL;

static bool address_in_filter(const bt_addr_le_t *addr)
{
    // TODO check if address is in address filter
	return false;
}

static bool parse_advertisement_data(struct bt_data *data, void *user_data)
{
    if(data->type == BT_DATA_MANUFACTURER_DATA){
		// TODO get uuid
        return false;
    }
    return true;
}

// Scanned device found callback
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
    int res = 0;
	char addr_str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	LOG_DBG("Device found: %s", addr_str);
    // check if address is in authorised filter
    res = address_in_filter(addr);
    if(!res){
        return;
    }

    bt_data_parse(ad, parse_advertisement_data, &res);
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

}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{

}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

void ble_thread_main(void)
{
	LOG_DBG("Start ble thread");

	int res = 0;

	LOG_DBG("Enable BLE");
	res = bt_enable(NULL);
	if(res){
		LOG_ERR("BLE enable fail (err %d)", res);
		return;
	}

	LOG_DBG("Start scan");
	start_scan();

	while(1){
		k_sleep(K_MSEC(1000));
	}
}

// create and start BLE thread
K_THREAD_DEFINE(ble_thread, 1024, ble_thread_main, NULL, NULL, NULL, 1, K_ESSENTIAL, 0);
