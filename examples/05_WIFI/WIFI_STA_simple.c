#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_err.h"
#include "esp_event.h"
#include "nvs_flash.h"

// EventGroupHandle_t event_group;
void event_handler(    void* event_handler_arg,
                            esp_event_base_t event_base,
                            int32_t event_id,
                            void* event_data);

#define CONNECTED_BIT BIT0
#define TIME_RECONNECT_MAX 10
uint8_t time_reconnect = 0;
void app_main() {
    // Initialize Non-Volatile Storage
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_ERROR_CHECK(esp_netif_init());
    // WIFI_EVENT_STA_START, WIFI_EVENT_STA_CONNECTED, WIFI_EVENT_SCAN_DONE
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL, NULL));
    
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = ""
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // event_group = xEventGroupCreate();

    printf("wait for connected\n");
    // xEventGroupWaitBits(event_group, CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void event_handler(     void* event_handler_arg,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void* event_data) {
    if(event_base==WIFI_EVENT) {
        switch (event_id)
        {
            case WIFI_EVENT_STA_START:
                printf("WIFI_EVENT started\n");
                time_reconnect = 0;
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_CONNECTED:
                printf("WIFI is connected, and request for DHCP\n");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t*) event_data;
                printf("Connection failed, due to: %d\n", event->reason);
                time_reconnect++;
                vTaskDelay(pdMS_TO_TICKS(500));

                if(time_reconnect>TIME_RECONNECT_MAX) {
                    printf("retry connect over %d times\n", TIME_RECONNECT_MAX);
                }
                else {
                    printf("#%d reconnecting...\n", time_reconnect);
                    esp_wifi_connect();
                }

                break;
            default:
                break;
        }
    }

    if(event_base==IP_EVENT) {
        switch (event_id)
        {
            case IP_EVENT_STA_GOT_IP:
                // xEventGroupSetBits(event_group, CONNECTED_BIT);
                ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                printf("成功取得 IP！\n");
                printf("IP Address:  " IPSTR "\n", IP2STR(&event->ip_info.ip));
                printf("Subnet mask: " IPSTR "\n", IP2STR(&event->ip_info.netmask));
                printf("Gateway:     " IPSTR "\n", IP2STR(&event->ip_info.gw));
                time_reconnect = 0;
                break;
            case IP_EVENT_STA_LOST_IP:
                // xEventGroupClearBits(event_group, CONNECTED_BIT);
            
                break;
            default:
                break;
        }
    }
     
}