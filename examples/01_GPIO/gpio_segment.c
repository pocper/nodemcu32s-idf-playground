#include <stdio.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"

void display_all();
void display_none();
void display(int num);

#define SEG_num 8
int SEG_GPIO[SEG_num] = {0, 4, 5, 15, 16, 17, 18, 19};
int SEG_val_none[SEG_num] = {1, 1, 1, 1, 1, 1, 1, 1};
int SEG_val_all[SEG_num]  = {0, 0, 0, 0, 0, 0, 0, 0};
int SEG_val_0[SEG_num]    = {0, 0, 0, 0, 1, 1, 0, 0};
int SEG_val_1[SEG_num]    = {1, 0, 1, 0, 1, 1, 1, 1};
int SEG_val_2[SEG_num]    = {0, 1, 1, 0, 1, 0, 0, 0};
int SEG_val_3[SEG_num]    = {0, 0, 1, 0, 1, 0, 0, 1};
int SEG_val_4[SEG_num]    = {1, 0, 0, 0, 1, 0, 1, 1};
int SEG_val_5[SEG_num]    = {0, 0, 0, 1, 1, 0, 0, 1};
int SEG_val_6[SEG_num]    = {0, 0, 0, 1, 1, 0, 0, 0};
int SEG_val_7[SEG_num]    = {1, 0, 0, 0, 1, 1, 0, 1};
int SEG_val_8[SEG_num]    = {0, 0, 0, 0, 1, 0, 0, 0};
int SEG_val_9[SEG_num]    = {0, 0, 0, 0, 1, 0, 0, 1};

void app_main() {
    gpio_reset_pin(15);
    for(int i=0;i<SEG_num;i++) {
        gpio_set_direction(SEG_GPIO[i], GPIO_MODE_OUTPUT);
    }
    
    while(1) {
        display_none();
        vTaskDelay(500/portTICK_PERIOD_MS);
        for(int i=0;i<10;i++) {
            display(i);
            vTaskDelay(500/portTICK_PERIOD_MS);
        }
        display_all();
        vTaskDelay(500/portTICK_PERIOD_MS);
    }
}
void display_none() {
    for(int i=0;i<SEG_num;i++) {
        gpio_set_level(SEG_GPIO[i], SEG_val_none[i]);
    }
}

void display_all() {
    for(int i=0;i<SEG_num;i++) {
        gpio_set_level(SEG_GPIO[i], SEG_val_all[i]);
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
        case 2:
            gpio_set_level(SEG_GPIO[i], SEG_val_2[i]);
            break;
        case 3:
            gpio_set_level(SEG_GPIO[i], SEG_val_3[i]);
            break;
        case 4:
            gpio_set_level(SEG_GPIO[i], SEG_val_4[i]);
            break;
        case 5:
            gpio_set_level(SEG_GPIO[i], SEG_val_5[i]);
            break;
        case 6:
            gpio_set_level(SEG_GPIO[i], SEG_val_6[i]);
            break;
        case 7:
            gpio_set_level(SEG_GPIO[i], SEG_val_7[i]);
            break;
        case 8:
            gpio_set_level(SEG_GPIO[i], SEG_val_8[i]);
            break;
        case 9:
            gpio_set_level(SEG_GPIO[i], SEG_val_9[i]);
            break;
        }
    }
}