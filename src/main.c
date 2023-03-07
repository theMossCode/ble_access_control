#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ble/ble.h"
#include "output.h"
#include "storage.h"

LOG_MODULE_REGISTER(MAIN);

void main()
{
	LOG_DBG("Start main thread");
	while(1){
		k_sleep(K_MSEC(1000));
	}
}