#ifndef BLE_H
#define BLE_H

#include "main.h"
#include <zephyr/bluetooth/addr.h>

enum ble_message_types{
    BLE_MSG_TYPE_ENABLE_AUTHENTICATION,
    BLE_MSG_TYPE_START_AUTHENTICATION,
    BLE_MSG_TYPE_STOP_AUTHENTICATION,
    BLE_MSG_START_SCAN,
};

struct ble_msg{
    int type;
};

/**
 * @brief BLE thread entry function
 * 
 */
void ble_thread_main();

/**
 * @brief Add address to address filter
 * 
 * @param addr address to add
 * @return 0 on success
 */
int ble_add_addr_to_filter(bt_addr_le_t *addr);

#endif