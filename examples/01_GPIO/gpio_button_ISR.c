#include <stdio.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"

void ISR_handler(void*);

SemaphoreHandle_t xSemaphore = NULL;

void app_main() {
    gpio_set_direction(21, GPIO_MODE_INPUT);
    ESP_ERROR_CHECK(gpio_set_intr_type(21, GPIO_INTR_NEGEDGE));
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1));
    ESP_ERROR_CHECK(gpio_isr_handler_add(21, &ISR_handler, NULL));
    xSemaphore = xSemaphoreCreateBinary();
    printf("start program\n");
    while (1)
    {
        if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
            printf("received button pressed\n");
        }
    }
}

void IRAM_ATTR ISR_handler(void* arg) {
    xSemaphoreGiveFromISR(xSemaphore, NULL);
}
