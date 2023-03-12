#include "uart.h"

#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/data/json.h>

#include <zephyr/sys/reboot.h>

#define UART_NODE       DT_NODELABEL(uart0)

LOG_MODULE_REGISTER(UART);

#define MSG_WAIT_TIMEOUT_MS                         5000

#define UART_MSG_TYPE_UPDATE_MAC                    0x00
#define UART_MSG_TYPE_UPDATE_SETTINGS               0x01
#define UART_MSG_TYPE_UNKNOWN                       0xff

#define UART_RESP_STATUS_DECODE_SUCCESS             0x01
#define UART_RESP_STATUS_DECODE_FAIL                0x00

enum msg_rx_states{
    MSG_START_RX, MSG_RX_IN_PROGRESS, MSG_START_DECODING, MSG_DECODING 
};

K_SEM_DEFINE(uart_msg_received_sem, 0, 1);

const struct device *uart_device = DEVICE_DT_GET(UART_NODE);

char rx_buffer[500];
int json_len;
int rx_state = MSG_START_RX;
int64_t msg_rx_start_time = 0;

struct update_msg{
    const char *ID;
    int Type; // If 1, settings, if 0 mac address(Tags)
    int Count; 
    const char *Tags[25];
    int TL; // Overhead light timeout(Minutes)
    int TS; // Scan timeout(Seconds)
    const char *VC; // Voltage cut off (volts)
    int VW; // Voltage wake(hours)
};

const struct json_obj_descr update_msg_descr[] = {
    JSON_OBJ_DESCR_PRIM(struct update_msg, ID, JSON_TOK_STRING),
    JSON_OBJ_DESCR_PRIM(struct update_msg, Type, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_ARRAY(struct update_msg, Tags, 25, Count, JSON_TOK_STRING),
    JSON_OBJ_DESCR_PRIM(struct update_msg, TL, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM(struct update_msg, TS, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM(struct update_msg, VC, JSON_TOK_STRING),
    JSON_OBJ_DESCR_PRIM(struct update_msg, VW, JSON_TOK_NUMBER),
};

static int send_uart_resp(uint8_t msg_type, uint8_t status)
{
    uint8_t tx_buffer[4];
    tx_buffer[0] = 0x02;
    tx_buffer[1] = msg_type;
    tx_buffer[2] = status;
    tx_buffer[3] = 0x03;

	for(int i=0; i<sizeof(tx_buffer); ++i) {
		uart_poll_out(uart_device, tx_buffer[i]);
	}

    return 0;
}

static unsigned char ascii_to_hex( char ch )
{
    if (('0' <= ch) && (ch <= '9')){
        ch -= '0';
    }
    else{
        if (('a' <= ch) && (ch <= 'f')){
            ch += 10 - 'a';
        }
        else{
            if (('A' <= ch) && (ch <= 'F')){
                ch += 10 - 'A';
            }
            else{
                ch = 16;
            }
        }
    }
    return ch;
}

static float str_to_float(const char *str)
{
    float res = 0;
    float scale = 0;
    while(*str){
        if(*str >= '0' && *str <= '9'){
            res = 10 * (res + (*str - '0'));
            scale *= 10;
        }
        else if(*str == '.'){
            scale = 10;
        }
        str++;
    }

    return (res / (scale == 0 ? 1 : scale));
}

static int decode_uart_msg()
{
    LOG_DBG("parse msg\n%s\nof size%d", rx_buffer, json_len);
    struct update_msg temp_msg;
    int ret = json_obj_parse(rx_buffer, json_len, update_msg_descr, 
                            ARRAY_SIZE(update_msg_descr), &temp_msg);

    if(ret < 0){
        LOG_ERR("JSON parse error %d", ret);
        send_uart_resp(UART_MSG_TYPE_UNKNOWN, UART_RESP_STATUS_DECODE_FAIL);
        rx_state = MSG_START_RX;
        return ret;
    }

    // MAC address
    if(temp_msg.Type == 0){
        for(int i=0; i<temp_msg.Count; ++i){
            LOG_DBG("Addr %d: %s", i, temp_msg.Tags[i]);
            bt_addr_le_t temp_addr = {
                .type = BT_ADDR_LE_RANDOM,
            };

            int mac_index = 0;
            for(int j=0; j<12; j+=2){
                char mac_hex = ascii_to_hex(temp_msg.Tags[i][j]);
                mac_hex <<= 4;
                mac_hex |= ascii_to_hex(temp_msg.Tags[i][j+1]);
                temp_addr.a.val[mac_index++] = mac_hex;
            }

            char addr_str[BT_ADDR_LE_STR_LEN];
            bt_addr_le_to_str(&temp_addr, addr_str, BT_ADDR_STR_LEN);
            LOG_DBG("Decoded address: %s", addr_str);

            ret = write_nvs_data(i, temp_addr.a.val, 6);
            if(ret){
                LOG_ERR("Add MAC to flash error");
                send_uart_resp(UART_MSG_TYPE_UPDATE_MAC, UART_RESP_STATUS_DECODE_FAIL);
                rx_state = MSG_START_RX;
                return ret;
            }
        }
        send_uart_resp(UART_MSG_TYPE_UPDATE_MAC, UART_RESP_STATUS_DECODE_SUCCESS);
        // TODO notify main of update
    }
    // Variables
    else if(temp_msg.Type == 1){
        float vc = str_to_float(temp_msg.VC);

        ret = update_uicr(temp_msg.TL, temp_msg.TS, (uint32_t)(vc * 100), temp_msg.VW);
        if(ret){
            send_uart_resp(UART_MSG_TYPE_UPDATE_SETTINGS, UART_RESP_STATUS_DECODE_FAIL);
            rx_state = MSG_START_RX;
            return ret;
        }

        send_uart_resp(UART_MSG_TYPE_UPDATE_SETTINGS, UART_RESP_STATUS_DECODE_SUCCESS);
        LOG_INF("Settings updated");
        // TODO notify main to reboot
    }

    rx_state = MSG_START_RX;
    return 0;
}

static int get_uart_msg()
{
    while(uart_irq_rx_ready(uart_device)){
        char c;
        uart_fifo_read(uart_device, &c, 1);
        switch(rx_state){
            case MSG_START_RX:{
                if(c != 0x02){
                    LOG_ERR("Invalid start tag");
                    return -EINVAL;
                }
                rx_state = MSG_RX_IN_PROGRESS;
                json_len = 0;
                msg_rx_start_time = k_uptime_get();
                memset(rx_buffer, 0, sizeof(rx_buffer));
                break;
            }
            case MSG_RX_IN_PROGRESS:{
                if(c == 0x03){
                    rx_state = MSG_START_DECODING;
                    LOG_DBG("Message end");
                    return 0;
                }
                else if(c == 0x20){
                    // Ignore whitespace
                    break;
                }

                rx_buffer[json_len++] = c;
                break;
            }
            case MSG_DECODING:{
                return -EALREADY;
            }
            default:{
                return -EINVAL;
            }
        }
    }

    return -ENOMSG;
}

static void uart_irq_callback(const struct device *dev, void *userdata)
{
    int err = 0;
    err = uart_irq_update(uart_device);
    if(err != 1){
        LOG_ERR("UART update error %d", err);
        return;
    }

    get_uart_msg();
    if(rx_state == MSG_START_DECODING){
        k_sem_give(&uart_msg_received_sem);
        rx_state = MSG_DECODING;
        LOG_DBG("UART update message received %s", rx_buffer);
    }
}

static int init_uart()
{
    if(!device_is_ready(uart_device)){
        LOG_ERR("UART not ready");
        return -ENODEV;
    }

    uart_irq_callback_user_data_set(uart_device, uart_irq_callback, NULL);
    uart_irq_rx_enable(uart_device);

    return 0;
}

void uart_thread_main()
{
    LOG_DBG("Start uart thread");;
    if(init_uart()){
        return;
    }

    int err = 0;
    while(1){
        err = k_sem_take(&uart_msg_received_sem, K_MSEC(MSG_WAIT_TIMEOUT_MS));
        if(err){
            if(k_uptime_get() - msg_rx_start_time > 5000 && rx_state == MSG_RX_IN_PROGRESS){
                rx_state = MSG_START_RX;
            }
            continue;
        }
        err = decode_uart_msg();
        if(err){
            LOG_ERR("decode uart msg error");
            continue;
        }

        LOG_INF("Settings updated, reboot...");
        // wait for uart transmission to end
        k_busy_wait(50000);
        sys_reboot(SYS_REBOOT_COLD);
    }
}

K_THREAD_DEFINE(uart_thread, 1024, uart_thread_main, NULL, NULL, NULL, 1, K_ESSENTIAL, 0);