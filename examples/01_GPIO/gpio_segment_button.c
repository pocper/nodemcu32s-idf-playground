#include <stdio.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"

void display(int num);

#define SEG_num 8
int SEG_GPIO[SEG_num] = {0, 4, 5, 15, 16, 17, 18, 19};
int SEG_val_0[SEG_num]    = {0, 0, 0, 0, 1, 1, 0, 0};
int SEG_val_1[SEG_num]    = {1, 0, 1, 0, 1, 1, 1, 1};

void app_main() {
    gpio_reset_pin(15);
    gpio_set_direction(21, GPIO_MODE_INPUT);
    for(int i=0;i<SEG_num;i++) {
        gpio_set_direction(SEG_GPIO[i], GPIO_MODE_OUTPUT);
    }
    int btn = 0;
    while(1) {
        btn = gpio_get_level(21);
        display(btn);
    }
}

void display(int num) {
    for(int i=0;i<SEG_num;i++) {
        switch (num)
        {
        case 0:
            gpio_set_level(SEG_GPIO[i], SEG_val_0[i]);
            break;
        case 1:
            gpio_set_level(SEG_GPIO[i], SEG_val_1[i]);
            break;
        }
    }
}