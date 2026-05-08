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

EventGroupHandle_t event_group;
void wifi_event_handler(void* event_handler_arg,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void* event_data);
void ip_event_handler(  void* event_handler_arg,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void* event_data);
void ping_success(esp_ping_handle_t hdl, void *args);
void ping_timeout(esp_ping_handle_t hdl, void *args);

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
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler, NULL, NULL));
    
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            }
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(40));

    event_group = xEventGroupCreate();

    printf("wait for connected\n");
    xEventGroupWaitBits(event_group, CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ip_addr_t target_addr;
    ipaddr_aton("8.8.8.8", &target_addr); // 直接測試外網 IP
    // ipaddr_aton("10.182.20.109", &target_addr); // 直接測試外網 IP
    // ipaddr_aton("192.168.1.107", &target_addr); // 直接測試外網 IP
    // ipaddr_aton("192.168.1.1", &target_addr); // 直接測試外網 IP
    target_addr.type = IPADDR_TYPE_V4;
    ping_config.target_addr = target_addr;
    ping_config.count = ESP_PING_COUNT_INFINITE;

    
    esp_ping_callbacks_t cbs;
    cbs.on_ping_success = ping_success;
    cbs.on_ping_timeout = ping_timeout;

    esp_ping_handle_t ping;
    esp_ping_new_session(&ping_config, &cbs, &ping);
    esp_ping_start(ping);
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
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

void ping_success(esp_ping_handle_t hdl, void *args) {
    uint32_t elapsed_time;
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    printf("Ping 成功: 耗時 %lu ms\n", elapsed_time);
}

void ping_timeout(esp_ping_handle_t hdl, void *args) {
    printf("Ping 逾時 (Timeout)\n");
}