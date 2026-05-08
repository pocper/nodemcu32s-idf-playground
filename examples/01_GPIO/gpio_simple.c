#include <stdio.h>
#include <stdint.h>

#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "driver/gpio.h"

void app_main() {
    gpio_reset_pin(2);
    gpio_set_direction(2, GPIO_MODE_OUTPUT);

    while(1) {
        gpio_set_level(2, 1);
        vTaskDelay(1000/portTICK_PERIOD_MS); // Delay 1s
        gpio_set_level(2, 0);
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}