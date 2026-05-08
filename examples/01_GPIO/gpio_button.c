#include <stdio.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"

void app_main() {
    gpio_set_direction(21, GPIO_MODE_INPUT);
    int btn = 0;
    while(1) {
        btn = gpio_get_level(21);
        printf("btn = %d\n", btn);
        vTaskDelay(500/portTICK_PERIOD_MS);
    }
}
