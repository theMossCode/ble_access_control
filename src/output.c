#include "output.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(OUTPUT);

void output_thread_main()
{
    LOG_DBG("Start output thread");
    while(1){
        k_sleep(K_MSEC(1000));
    }
}

K_THREAD_DEFINE(output_thread, 1024, output_thread_main, NULL, NULL, NULL, 2, K_ESSENTIAL, 0);