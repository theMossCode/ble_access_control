#ifndef OUTPUT_H
#define OUTPUT_H

#include "main.h"

#define LED_STATE_ON                    1
#define LED_STATE_OFF                   0

#define LIGHT_STATE_ON                  1
#define LIGHT_STATE_OFF                 0

enum output_msg_types{
    OUTPUT_MSG_TYPE_TOGGLE_OVERHEAD_LIGHT, 
    OUTPUT_MSG_TYPE_TOGGLE_LED, 
    OUTPUT_MSG_TYPE_TOGGLE_TAG_AUTHENTICATED, 
    OUTPUT_MSG_TOGGLE_TAG_AUTHENTICATION_FAIL
};

struct output_msg{
    int type;
    int state;
};

void output_thread_main();

#endif