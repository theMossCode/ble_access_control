#include "output.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(OUTPUT);

#define DEFAULT_TIMEOUT_FOR_LIGHT_SECONDS           (5 * 60)

#define LED_LIGHT_NODE                              DT_NODELABEL(led)
#define OVERHEAD_LIGHT_NODE                         DT_NODELABEL(out1)
#define AUTHENTICATED_LIGHT_NODE                    DT_NODELABEL(out2)
#define AUTHENTICATION_FAIL_LIGHT_NODE              DT_NODELABEL(out3)

struct output_light{
    struct gpio_dt_spec gpio_spec;
    int current_state;
};

static void overhead_light_timer_exp_cb(struct k_timer *timer);

K_MSGQ_DEFINE(output_msgq, sizeof(struct output_msg), 5, 4);
K_TIMER_DEFINE(overhead_light_timer, overhead_light_timer_exp_cb, NULL);

struct output_light overhead_light = {
    .current_state = 0,
    .gpio_spec = GPIO_DT_SPEC_GET(OVERHEAD_LIGHT_NODE, gpios)  
};

struct output_light authenticated_light = {
    .current_state = 0,
    .gpio_spec = GPIO_DT_SPEC_GET(AUTHENTICATED_LIGHT_NODE, gpios)  
};

struct output_light authentication_fail_light = {
    .current_state = 0,
    .gpio_spec = GPIO_DT_SPEC_GET(AUTHENTICATION_FAIL_LIGHT_NODE, gpios)  
};

struct output_light led_light = {
    .current_state = 0,
    .gpio_spec = GPIO_DT_SPEC_GET(LED_LIGHT_NODE, gpios)
};

static int init_light(struct output_light *light, int initial)
{
    int ret = 0;
    if(!device_is_ready(light->gpio_spec.port)){
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&light->gpio_spec, GPIO_OUTPUT);
    if(ret){
        LOG_ERR("GPIO output init fail (error %d)", ret);
        return ret;
    }

    gpio_pin_set_dt(&light->gpio_spec, initial);

    return 0;
}

static void toggle_light(struct output_light *light, int state)
{
    // if(state == light->current_state){
    //     return;
    // }

    gpio_pin_set_dt(&light->gpio_spec, state);
}

static void overhead_light_timer_exp_cb(struct k_timer *timer)
{
    toggle_light(&overhead_light, 0);
    k_timer_stop(&overhead_light_timer);
}

void output_thread_main()
{
    LOG_DBG("Start output thread");

    if(init_light(&led_light, LED_STATE_OFF)){
        return;
    }

    if(init_light(&overhead_light, LIGHT_STATE_OFF)){
        return;
    }

    if(init_light(&authenticated_light, LIGHT_STATE_OFF)){
        return;
    }

    if(init_light(&authentication_fail_light, LIGHT_STATE_OFF)){
        return;
    }

    struct output_msg msg;
    while(1){
        k_msgq_get(&output_msgq, &msg, K_FOREVER);
        switch(msg.type){
            case OUTPUT_MSG_TYPE_TOGGLE_OVERHEAD_LIGHT:{
                toggle_light(&overhead_light, msg.state);
                if(msg.state == 0){
                    k_timer_stop(&overhead_light_timer);
                }
                else{
                    k_timer_start(&overhead_light_timer, K_SECONDS(DEFAULT_TIMEOUT_FOR_LIGHT_SECONDS), K_NO_WAIT);
                }

                break;
            }
            case OUTPUT_MSG_TYPE_TOGGLE_LED:{
                toggle_light(&led_light, msg.state);
                break;
            }
            case OUTPUT_MSG_TYPE_TOGGLE_TAG_AUTHENTICATED:{
                toggle_light(&authenticated_light, msg.state);
                break;
            }
            case OUTPUT_MSG_TOGGLE_TAG_AUTHENTICATION_FAIL:{
                toggle_light(&authentication_fail_light, msg.state);
                break;
            }
            default:{
                break;
            }
        }
    }
}

K_THREAD_DEFINE(output_thread, 1024, output_thread_main, NULL, NULL, NULL, 2, K_ESSENTIAL, 0);