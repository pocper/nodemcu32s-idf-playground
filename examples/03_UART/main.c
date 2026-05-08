#include <stdio.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "esp_err.h"

#define UART_TX 17
#define UART_RX 16

QueueHandle_t uart_queue;
void uart_rx_received(void *arg);

void app_main() {
    printf("hello world\n");

    uart_config_t uart_cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, UART_TX, UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, 200, 200, 20, &uart_queue, ESP_INTR_FLAG_LOWMED));
    xTaskCreate(uart_rx_received, "uart rx", 4096, NULL, 1, NULL);

    char string[] = "hello world!";
    while(1) {
        printf("Send: %s\n", string);
        uart_write_bytes(UART_NUM_2, string, sizeof(string));
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void uart_rx_received(void *arg) {
    uart_event_t event;
    char string_received[128];
    while(1) {
        if(xQueueReceive(uart_queue, (void*)&event, portMAX_DELAY)) {
            switch (event.type)
            {
                case UART_DATA:
                    len = uart_read_bytes(UART_NUM_2, &string_received, event.size, portMAX_DELAY);
                    printf("Received: %s\n", string_received);
                    break;
                
                default:
                    break;
            }
        }
    }
}