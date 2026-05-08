#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "ping/ping_sock.h"
// #include "esp_crt_bundle.h"

EventGroupHandle_t event_group;
void wifi_event_handler(void* event_handler_arg,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void* event_data);
void ip_event_handler(  void* event_handler_arg,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void* event_data);
esp_err_t http_event_handler(esp_http_client_event_t *evt);

#define CONNECTED_BIT BIT0
#define TIME_RECONNECT_MAX 50
uint8_t time_reconnect = 0;
void app_main() {
    // Initialize Non-Volatile Storage
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_ERROR_CHECK(esp_netif_init());
    // WIFI_EVENT_STA_START, WIFI_EVENT_STA_CONNECTED, WIFI_EVENT_SCAN_DONE
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler, NULL, NULL));
    
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
            .threshold.authmode = WIFI_AUTH_WPA2_PSK
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    // ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    // ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(40));

    event_group = xEventGroupCreate();

    printf("wait for connected\n");
    xEventGroupWaitBits(event_group, CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    

    esp_http_client_config_t http_config = {
        // .url = "http://connectivitycheck.gstatic.com/generate_204",
        // .url = "https://httpbin.org/get",
        .url = "http://httpbin.org/get",
        // .url = "https://www.lucadentella.it/demo/aphorisms.php",
        .timeout_ms = 5000,
        .method = HTTP_METHOD_GET,
        .event_handler = http_event_handler
        // .crt_bundle_attach = esp_crt_bundle_attach
    };
    
    while(1) {
        esp_http_client_handle_t http_client = esp_http_client_init(&http_config);
        esp_err_t err = esp_http_client_perform(http_client);
        if(err==ESP_OK) {
            printf("Success, Status Code = %d\n", esp_http_client_get_status_code(http_client));
        }
        ESP_ERROR_CHECK(esp_http_client_cleanup(http_client));
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void wifi_event_handler(void* event_handler_arg,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void* event_data) {
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
            vTaskDelay(pdMS_TO_TICKS(3000));

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

void ip_event_handler(  void* event_handler_arg,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void* event_data) {
    switch (event_id)
    {
        case IP_EVENT_STA_GOT_IP:
            xEventGroupSetBits(event_group, CONNECTED_BIT);
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            printf("Received IP!\n");
            printf("IP Address:  " IPSTR "\n", IP2STR(&event->ip_info.ip));
            printf("Subnet mask: " IPSTR "\n", IP2STR(&event->ip_info.netmask));
            printf("Gateway:     " IPSTR "\n", IP2STR(&event->ip_info.gw));
            // ARP needs to wait for AP MAC address
            vTaskDelay(pdMS_TO_TICKS(5000));
            time_reconnect = 0;
            break;
        case IP_EVENT_STA_LOST_IP:
            xEventGroupClearBits(event_group, CONNECTED_BIT);
            time_reconnect = 0;
            break;
        default:
            break;
    }
}

esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            printf("HTTP connection success!\n");
            break;
        case HTTP_EVENT_ON_DATA:
            printf("收到資料長度: %d\n", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                printf("%.*s\n", evt->data_len, (char*)evt->data);
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}