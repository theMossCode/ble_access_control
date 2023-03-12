#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "main.h"

LOG_MODULE_REGISTER(MAIN, 3);

K_EVENT_DEFINE(main_evts);

extern struct k_msgq ble_msgq;
extern struct k_msgq output_msgq;

uint32_t timeout_for_scans_seconds = 15;

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

	evts = k_event_wait(&main_evts, MAIN_EVT_BLE_DEVICE_CONNECTED, true, K_SECONDS(timeout_for_scans_seconds));
	if(!evts){
		LOG_WRN("Connect device timeout");
		toggle_output(OUTPUT_MSG_TYPE_TOGGLE_LED, LED_STATE_OFF);
		toggle_output(OUTPUT_MSG_TOGGLE_TAG_AUTHENTICATION_FAIL, LIGHT_STATE_ON);
		update_authentication_state(BLE_MSG_TYPE_STOP_AUTHENTICATION);
	}
	else{
		update_authentication_state(BLE_MSG_TYPE_START_AUTHENTICATION);
		evts = k_event_wait(&main_evts, MAIN_EVT_BLE_DEVICE_AUTHENTICATED | MAIN_EVT_BLE_DEVICE_AUTHENTICATION_FAIL, true, K_SECONDS(15));
		if(!evts){
			LOG_WRN("Authentication timeout");
			toggle_output(OUTPUT_MSG_TOGGLE_TAG_AUTHENTICATION_FAIL, LIGHT_STATE_ON);
		}
		else if(evts & MAIN_EVT_BLE_DEVICE_AUTHENTICATED){
			toggle_output(OUTPUT_MSG_TYPE_TOGGLE_TAG_AUTHENTICATED, LIGHT_STATE_ON);
		}
		else if(evts & MAIN_EVT_BLE_DEVICE_AUTHENTICATION_FAIL){
			toggle_output(OUTPUT_MSG_TOGGLE_TAG_AUTHENTICATION_FAIL, LIGHT_STATE_ON);
		}

		update_authentication_state(BLE_MSG_TYPE_STOP_AUTHENTICATION);
	}
	
	evts = k_event_wait(&main_evts, MAIN_EVT_BTN_RELEASED, false, K_SECONDS(timeout_for_scans_seconds));
	// all lights off
	toggle_output(OUTPUT_MSG_TYPE_TOGGLE_LED, LED_STATE_OFF);
	toggle_output(OUTPUT_MSG_TYPE_TOGGLE_OVERHEAD_LIGHT, LIGHT_STATE_OFF);
	toggle_output(OUTPUT_MSG_TYPE_TOGGLE_TAG_AUTHENTICATED, LIGHT_STATE_OFF);
	toggle_output(OUTPUT_MSG_TOGGLE_TAG_AUTHENTICATION_FAIL, LIGHT_STATE_OFF);

	update_authentication_state(BLE_MSG_START_SCAN);
}

void main()
{
	LOG_DBG("Start main thread");

	if(init_nvs()){
		return;
	}

	read_uicr(TS_UICR_ADDRESS, &timeout_for_scans_seconds);
	if(timeout_for_scans_seconds >= 0xffff){
		LOG_WRN("Scan timeout value not valid, defaulting to 15 seconds");
		timeout_for_scans_seconds = 15;
	}

	if(input_init()){
		return;
	}

	uint32_t evts = 0;
	
	while(1){
		evts = k_event_wait(&main_evts, MAIN_EVT_BLE_DEVICE_FOUND | MAIN_EVT_BTN_PRESSED, true, K_SECONDS(DEFAULT_OVERHEAD_LIGHT_TIMEOUT_NO_SCAN));
		if(!evts){
			LOG_DBG("no scan data");
			toggle_output(OUTPUT_MSG_TYPE_TOGGLE_OVERHEAD_LIGHT, LIGHT_STATE_OFF);
		}
		else if(evts & MAIN_EVT_BLE_DEVICE_FOUND){
			LOG_DBG("Device found");
			toggle_output(OUTPUT_MSG_TYPE_TOGGLE_OVERHEAD_LIGHT, LIGHT_STATE_ON);
		}
		else if(evts & MAIN_EVT_BTN_PRESSED){
			LOG_DBG("BTN pressed, start authentication");

			toggle_output(OUTPUT_MSG_TYPE_TOGGLE_LED, LED_STATE_ON);
			toggle_output(OUTPUT_MSG_TYPE_TOGGLE_OVERHEAD_LIGHT, LIGHT_STATE_OFF);
			update_authentication_state(BLE_MSG_TYPE_ENABLE_AUTHENTICATION);
			wait_authentication();
		}
	}
}