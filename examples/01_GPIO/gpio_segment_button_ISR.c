#include <stdio.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"

void display(int num);
void ISR_handler(void*);

#define SEG_num 8
int SEG_GPIO[SEG_num] = {0, 4, 5, 15, 16, 17, 18, 19};
int SEG_val_0[SEG_num]    = {0, 0, 0, 0, 1, 1, 0, 0};
int SEG_val_1[SEG_num]    = {1, 0, 1, 0, 1, 1, 1, 1};

QueueHandle_t xQueue;

void app_main() {
    gpio_reset_pin(15);
    for(int i=0;i<SEG_num;i++) {
        gpio_set_direction(SEG_GPIO[i], GPIO_MODE_OUTPUT);
    }
    gpio_set_direction(21, GPIO_MODE_INPUT);
    ESP_ERROR_CHECK(gpio_set_intr_type(21, GPIO_INTR_ANYEDGE));
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1));
    ESP_ERROR_CHECK(gpio_isr_handler_add(21, &ISR_handler, NULL));
    xQueue = xQueueCreate(10, sizeof(int));
    printf("start program\n");
    int btn_val = 1;
    display(btn_val);
    while (1)
    {
        if(xQueueReceive(xQueue, &btn_val, portMAX_DELAY) == pdTRUE) {
            display(btn_val);
        }
    }
}

void IRAM_ATTR ISR_handler(void* arg) {
    int btn_val = gpio_get_level(21);
    xQueueSendFromISR(xQueue, &btn_val, NULL);
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