#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_err.h"
#include "esp_event.h"
#include "nvs_flash.h"

void WIFI_handler(  void* event_handler_arg,
                    esp_event_base_t event_base,
                    int32_t event_id,
                    void* event_data);
QueueHandle_t xSem;

void app_main() {
    // Initialize Non-Volatile Storage
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_ERROR_CHECK(esp_netif_init());
    // WIFI_EVENT_STA_START, WIFI_EVENT_STA_CONNECTED, WIFI_EVENT_SCAN_DONE
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WIFI_handler, NULL, NULL));

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    const wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = true
    };
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, false));

    uint16_t ap_num = 20;
    wifi_ap_record_t ap_records[20];
    xSem = xSemaphoreCreateBinary();
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if(xSemaphoreTake(xSem, pdMS_TO_TICKS(1))==pdTRUE) {
            esp_wifi_scan_get_ap_records(&ap_num, ap_records);
            for(int i=0;i<ap_num;i++) {
                printf("[%d] | SSID=%s, RSSI=%d\n", i, ap_records[i].ssid, ap_records[i].rssi);
            }
        }
    }
}

void WIFI_handler(  void* event_handler_arg,
                    esp_event_base_t event_base,
                    int32_t event_id,
                    void* event_data) {
    
    if(event_id==WIFI_EVENT_SCAN_DONE) {
        xSemaphoreGive(xSem);
    }
}