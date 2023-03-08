#include "input.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(INPUT);

#define INPUT_BTN_NODE DT_NODELABEL(button_input)

extern struct k_event main_evts;

struct gpio_dt_spec input_btn_spec = GPIO_DT_SPEC_GET(INPUT_BTN_NODE, gpios); 
struct gpio_callback input_btn_callback;

static void input_btn_pressed_callback(const struct device *dev, struct gpio_callback *cb, gpio_port_pins_t pins)
{
    k_event_set(&main_evts, MAIN_EVT_BTN_PRESSED);
}

int input_init()
{
    int ret = 0;
    if(!device_is_ready(input_btn_spec.port)){
        LOG_ERR("Button input not ready");
        return -ENODEV;
    }

    gpio_init_callback(&input_btn_callback, input_btn_pressed_callback, BIT(input_btn_spec.pin));
    ret = gpio_pin_interrupt_configure_dt(&input_btn_spec, GPIO_INT_EDGE_TO_ACTIVE);
    if(ret){
        LOG_ERR("Input btn interrupt config fail (err %d)", ret);
        return ret;
    }

    ret = gpio_add_callback(input_btn_spec.port, &input_btn_callback);
    if(ret){
        LOG_ERR("Input btn add interrupt callback fail (err %d)", ret);
        return ret;
    }

    ret = gpio_pin_configure_dt(&input_btn_spec, GPIO_INPUT);
    if(ret){
        LOG_ERR("Input btn config fail (err %d)", ret);
        return ret;
    }

    ret = gpio_pin_set_dt(&input_btn_spec, 1);
    if(ret){
        LOG_ERR("input button init fail (err %d)", ret);
        return ret;
    }

    LOG_INF("Inputs initialised");
    return 0;
}