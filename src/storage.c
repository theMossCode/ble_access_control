#include "storage.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(STORAGE);

void storage_thread_main()
{
    LOG_DBG("Start storage thread");
    while(1){
        k_sleep(K_MSEC(1000));
    }
}

K_THREAD_DEFINE(storage_thread, 1024, storage_thread_main, NULL, NULL, NULL, 3, K_ESSENTIAL, 0);