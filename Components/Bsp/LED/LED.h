#ifndef _LED_H
#define _LED_H

#include "driver/gpio.h"
#define LED_GPIO_PIN GPIO_NUM_1

typedef enum {
    PIN_RESET,
    PIN_SET
} GPIO_OUTPUT_STATE;
#define LED(X)  gpio_set_level(LED_GPIO_PIN, X)
#define LED_TOGGER() gpio_set_level(LED_GPIO_PIN, !gpio_get_level(LED_GPIO_PIN))

void LED_Init(void);
#endif
