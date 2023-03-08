#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "main.h"

LOG_MODULE_REGISTER(MAIN);

K_EVENT_DEFINE(main_evts);

extern struct k_msgq ble_msgq;
extern struct k_msgq output_msgq;

static void update_authentication_state(int state)
{
	struct ble_msg msg = {
		.type = state
	};

	k_msgq_put(&ble_msgq, &msg, K_NO_WAIT);
}

static void toggle_output(int type, int state)
{
	struct output_msg out_msg = {
		.type = type,
		.state = state
	};

	k_msgq_put(&output_msgq, &out_msg, K_NO_WAIT);
}

static void wait_authentication()
{
	uint32_t evts = 0;

	evts = k_event_wait(&main_evts, MAIN_EVT_BLE_DEVICE_CONNECTED, true, K_SECONDS(10));
	if(!evts){
		LOG_ERR("Connect device timeout");
		toggle_output(OUTPUT_MSG_TYPE_TOGGLE_LED, LED_STATE_OFF);
		toggle_output(OUTPUT_MSG_TYPE_TOGGLE_OVERHEAD_LIGHT, LIGHT_STATE_OFF);
		toggle_output(OUTPUT_MSG_TOGGLE_TAG_AUTHENTICATION_FAIL, LIGHT_STATE_ON);

		update_authentication_state(BLE_MSG_TYPE_STOP_AUTHENTICATION);
		return;
	}

	update_authentication_state(BLE_MSG_TYPE_START_AUTHENTICATION);
	evts = k_event_wait(&main_evts, MAIN_EVT_BLE_DEVICE_AUTHENTICATED | MAIN_EVT_BLE_DEVICE_AUTHENTICATION_FAIL, true, K_SECONDS(15));
	if(!evts){
		LOG_ERR("Authentication timeout");
		toggle_output(OUTPUT_MSG_TOGGLE_TAG_AUTHENTICATION_FAIL, LIGHT_STATE_ON);
	}
	else if(evts & MAIN_EVT_BLE_DEVICE_AUTHENTICATED){
		toggle_output(OUTPUT_MSG_TYPE_TOGGLE_TAG_AUTHENTICATED, LIGHT_STATE_ON);
		toggle_output(OUTPUT_MSG_TYPE_TOGGLE_OVERHEAD_LIGHT, LIGHT_STATE_OFF);
		// TODO authenticated!
	}
	else if(evts & MAIN_EVT_BLE_DEVICE_AUTHENTICATION_FAIL){
		toggle_output(OUTPUT_MSG_TOGGLE_TAG_AUTHENTICATION_FAIL, LIGHT_STATE_ON);
		toggle_output(OUTPUT_MSG_TYPE_TOGGLE_OVERHEAD_LIGHT, LIGHT_STATE_OFF);
		// TODO authentication fail
	}

	update_authentication_state(BLE_MSG_TYPE_STOP_AUTHENTICATION);
	toggle_output(OUTPUT_MSG_TYPE_TOGGLE_LED, LED_STATE_OFF);
}

void main()
{
	LOG_DBG("Start main thread");

	if(input_init()){
		return;
	}

	uint32_t evts = 0;
	
	while(1){
		evts = k_event_wait(&main_evts, MAIN_EVT_BLE_DEVICE_FOUND | MAIN_EVT_BTN_PRESSED, true, K_SECONDS(DEFAULT_TIMEOUT_FOR_SCANS_SECONDS));
		if(!evts){
			LOG_INF("Scan timeout");
		}
		else if(evts & MAIN_EVT_BLE_DEVICE_FOUND){
			toggle_output(OUTPUT_MSG_TYPE_TOGGLE_OVERHEAD_LIGHT, LIGHT_STATE_ON);
		}
		else if(evts & MAIN_EVT_BTN_PRESSED){
			LOG_INF("BTN pressed, start authentication");

			toggle_output(OUTPUT_MSG_TYPE_TOGGLE_LED, LED_STATE_ON);
			update_authentication_state(BLE_MSG_TYPE_ENABLE_AUTHENTICATION);
			wait_authentication();
		}
	}
}