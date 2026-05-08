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
#include <time.h>
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
// #include "esp_crt_bundle.h"
#include <netdb.h>
#include <arpa/inet.h>

EventGroupHandle_t event_group;
void wifi_event_handler(void *event_handler_arg,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void *event_data);
void ip_event_handler(void *event_handler_arg,
                      esp_event_base_t event_base,
                      int32_t event_id,
                      void *event_data);
esp_err_t http_event_handler(esp_http_client_event_t *evt);
void time_sync_notification_cb(struct timeval *tv);
void ping_success(esp_ping_handle_t hdl, void *args);
void ping_timeout(esp_ping_handle_t hdl, void *args);
#define CONNECTED_BIT BIT0
#define SYNC_BIT      BIT1
#define TIME_RECONNECT_MAX 50
uint8_t time_reconnect = 0;
void app_main()
{
    esp_log_level_set("sntp", ESP_LOG_DEBUG);
    esp_log_level_set("esp_netif_sntp", ESP_LOG_DEBUG);

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
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    event_group = xEventGroupCreate();
    
    time_t now;
    setenv("TZ", "CST-8", 1);
    tzset();
    time(&now);
    printf("當前系統原始時間: %s", ctime(&now));

    
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("tw.pool.ntp.org");
    config.num_of_servers = 3;
    config.servers[1] = "tock.stdtime.gov.tw";
    config.servers[2] = "162.159.200.1";
    config.sync_cb = time_sync_notification_cb;
    config.start = false;
    EventBits_t bits;
    bool is_wifi_connected, is_sntp_enabled, is_sntp_synced;

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ip_addr_t target_addr;
    // FIXME: 需要先建立ping(ICMP)的連線，SNTP才會正常
    // ipaddr_aton("8.8.8.8", &target_addr); // 直接測試外網 IP
    ipaddr_aton("192.168.1.1", &target_addr); // 直接測試外網 IP
    target_addr.type = IPADDR_TYPE_V4;
    ping_config.target_addr = target_addr;
    ping_config.count = ESP_PING_COUNT_INFINITE;

    
    esp_ping_callbacks_t cbs;
    cbs.on_ping_success = ping_success;
    cbs.on_ping_timeout = ping_timeout;

    esp_ping_handle_t ping;
    esp_ping_new_session(&ping_config, &cbs, &ping);
    
    bool is_ping_started = false;

    while(1) {
        bits = xEventGroupGetBits(event_group);
        is_wifi_connected = (bits & CONNECTED_BIT);
        is_sntp_synced = (bits & SYNC_BIT);
        is_sntp_enabled = esp_sntp_enabled();

        if(is_wifi_connected && !is_ping_started) {
            is_ping_started = true;
            esp_ping_start(ping);
        }
        
        if(!is_wifi_connected && is_ping_started) {
            is_ping_started = false;
            esp_ping_stop(ping);
        }
        
        if(is_wifi_connected && !is_sntp_enabled) {
            printf("Starting SNTP...\n");
            esp_netif_sntp_init(&config);
            esp_netif_sntp_start();
        }

        if(!is_wifi_connected && is_sntp_enabled) {
            printf("Closing SNTP...\n");
            esp_netif_sntp_deinit();
        }

        if(is_sntp_synced) {
            time(&now);
            printf("系統當前時間: %s", ctime(&now));
        }
        printf("STATUS | WIFI = %d, SNTP = %d, SNTP_synced = %d\n", is_wifi_connected, is_sntp_enabled, is_sntp_synced);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void wifi_event_handler(void *event_handler_arg,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void *event_data)
{
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
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        printf("Connection failed, due to: %d\n", event->reason);
        time_reconnect++;
        vTaskDelay(pdMS_TO_TICKS(10000));

        if (time_reconnect > TIME_RECONNECT_MAX)
        {
            printf("retry connect over %d times\n", TIME_RECONNECT_MAX);
        }
        else
        {
            printf("#%d reconnecting...\n", time_reconnect);
            esp_wifi_connect();
        }
        break;
    default:
        break;
    }
}

void time_sync_notification_cb(struct timeval *tv)
{
    xEventGroupSetBits(event_group, SYNC_BIT);
    ESP_LOGI("SNTP", "Notification of a time synchronization event");
}

void ip_event_handler(void *event_handler_arg,
                      esp_event_base_t event_base,
                      int32_t event_id,
                      void *event_data)
{
    switch (event_id)
    {
    case IP_EVENT_STA_GOT_IP:
        xEventGroupSetBits(event_group, CONNECTED_BIT);
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
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

esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_CONNECTED:
        printf("HTTP connection success!\n");
        break;
    case HTTP_EVENT_ON_DATA:
        printf("收到資料長度: %d\n", evt->data_len);
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            printf("%.*s\n", evt->data_len, (char *)evt->data);
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

void ping_success(esp_ping_handle_t hdl, void *args) {
    uint32_t elapsed_time;
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    printf("Ping 成功: 耗時 %lu ms\n", elapsed_time);
}

void ping_timeout(esp_ping_handle_t hdl, void *args) {
    printf("Ping 逾時 (Timeout)\n");
}