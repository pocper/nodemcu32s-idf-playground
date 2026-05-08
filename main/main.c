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
#include "esp_crt_bundle.h"
#include <netdb.h>
#include <arpa/inet.h>
#include "esp_http_server.h"

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

httpd_handle_t start_webserver(void);
esp_err_t hello_get_handler(httpd_req_t *req);
#define CONNECTED_BIT   BIT0
#define TIME_SYNC_BIT   BIT1
#define TIME_RECONNECT_MAX 50
#define MAX_HTTP_RECV_CONTENT 1024
uint8_t time_reconnect = 0;

typedef struct {
    char *buffer;       // 存放字串的空間
    int buffer_len;     // 目前已存入的長度
} response_data_t;

void app_main()
{
    // Initialize Non-Volatile Storage
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_ERROR_CHECK(esp_netif_init());
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
    
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("tw.pool.ntp.org");
    config.num_of_servers = 3;
    config.servers[1] = "tock.stdtime.gov.tw";
    config.servers[2] = "162.159.200.1";
    config.sync_cb = time_sync_notification_cb;
    config.start = false;
    EventBits_t bits;
    bool is_wifi_connected, is_sntp_enabled, is_sntp_synced;
    bool is_wifi_server = false;

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ip_addr_t target_addr;
    ipaddr_aton("8.8.8.8", &target_addr); // 直接測試外網 IP
    target_addr.type = IPADDR_TYPE_V4;
    ping_config.target_addr = target_addr;
    ping_config.count = ESP_PING_COUNT_INFINITE;

    esp_ping_handle_t ping;
    esp_ping_new_session(&ping_config, NULL, &ping);
    bool is_ping_active = false;

    char *raw_buffer = (char *)malloc(MAX_HTTP_RECV_CONTENT);
    

    esp_http_client_config_t http_config = {
        .url = "http://www.lucadentella.it/demo/aphorisms.php",
        .host = "www.lucadentella.it",
        .user_agent = "ESP32",
        .method = HTTP_METHOD_GET,
        .buffer_size = 2048,      // 增加接收緩衝區
        .buffer_size_tx = 1024,   // 增加發送緩衝區
        .timeout_ms = 15000,      // 將超時拉長到 15 秒（TLS 握手在弱網下很慢）
        .event_handler = http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true
    };

    while(1) {
        bits = xEventGroupGetBits(event_group);
        is_wifi_connected = (bits & CONNECTED_BIT);
        is_sntp_synced = (bits & TIME_SYNC_BIT);
        is_sntp_enabled = esp_sntp_enabled();

        if(is_wifi_connected && !is_wifi_server) {
            is_wifi_server = true;
            printf("start web server\n");
            start_webserver();
        }


        if(is_wifi_connected && !is_ping_active && !is_sntp_synced) {
            is_ping_active = true;
            esp_ping_start(ping);
        }

        if( is_ping_active &&
        (!is_wifi_connected || is_sntp_synced)) {
            is_ping_active = false;
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

            // raw_buffer = (char *)malloc(1024);
            // response_data_t my_res_data = {
            //     .buffer = raw_buffer,
            //     .buffer_len = 0
            // };
            // http_config.user_data = &my_res_data;
            // esp_http_client_handle_t http_client = esp_http_client_init(&http_config);
            // esp_err_t err = esp_http_client_perform(http_client);
            // if(err==ESP_OK) {
            //     printf("Success, Status Code = %d\n", esp_http_client_get_status_code(http_client));
            // }
            // free(raw_buffer);
            // ESP_ERROR_CHECK(esp_http_client_cleanup(http_client));
            vTaskDelay(pdMS_TO_TICKS(3000));
        }

        if(!is_wifi_connected || !is_sntp_enabled || !is_sntp_synced) {
            printf("STATUS | WIFI = %d, SNTP = %d, SNTP_synced = %d\n", is_wifi_connected, is_sntp_enabled, is_sntp_synced);
        }
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
    xEventGroupSetBits(event_group, TIME_SYNC_BIT);
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
    response_data_t *res_data = (response_data_t *)evt->user_data;
    switch (evt->event_id)
    {
        case HTTP_EVENT_ON_CONNECTED:
            printf("HTTP connection success!\n");
            break;
        case HTTP_EVENT_ON_DATA:
            printf("收到資料長度: %d\n", evt->data_len);
            if ((res_data->buffer_len + evt->data_len) < MAX_HTTP_RECV_CONTENT) {
                    memcpy(res_data->buffer + res_data->buffer_len, evt->data, evt->data_len);
                    res_data->buffer_len += evt->data_len;
                    res_data->buffer[res_data->buffer_len] = '\0';
                } else {
                    printf("警告: Buffer 空間不足，資料被截斷\n");
                }
            break;
        case HTTP_EVENT_ON_FINISH:
            if(res_data->buffer_len>0){
                printf("Buffer length = %d\n", res_data->buffer_len);
                printf("Buffer char = %s\n", (char *)res_data->buffer);
            }
            break;
        case HTTP_EVENT_DISCONNECTED:
            res_data->buffer_len = 0;
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t hello_get_handler(httpd_req_t *req) {
    // 定義要顯示的內容
    const char* resp_str = "<html><body><h1>Hello from ESP32!</h1><p>這是你的第一個 HTTP Server 範例。</p></body></html>";
    
    // 設定編碼（如果要顯示中文，請加上 charset=utf-8）
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    
    // 發送回應
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    
    return ESP_OK;
}

esp_err_t time_get_handler(httpd_req_t *req) {
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    httpd_resp_set_type(req, "text/plain"); // 設定為純文字格式
    httpd_resp_send(req, time_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

const char* index_html = 
"<html><head><meta charset=\"UTF-8\">"
"<script>"
"  function updateTime() {"
"    fetch('/time')"  // AJAX 請求
"      .then(response => response.text())"
"      .then(data => {"
"        document.getElementById('current_time').innerHTML = data;"
"      });"
"  }"
"  setInterval(updateTime, 1000);" // 每 1000 毫秒 (1秒) 更新一次
"</script>"
"</head><body>"
"  <h1>ESP32 AJAX 即時時鐘</h1>"
"  <div style='font-size: 24px; color: blue;' id='current_time'>正在取得時間...</div>"
"</body></html>";

esp_err_t index_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* 定義 URI 結構體，連結路徑與函式 */
httpd_uri_t uri_hello = {
    .uri      = "/hello",
    .method   = HTTP_GET,
    .handler  = hello_get_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_index = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = index_get_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_time = {
    .uri      = "/time",
    .method   = HTTP_GET,
    .handler  = time_get_handler,
    .user_ctx = NULL
};

httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG(); // 使用預設配置 (Port 80)

    // 啟動 httpd 伺服器
    if (httpd_start(&server, &config) == ESP_OK) {
        // 註冊 URI 處理器
        httpd_register_uri_handler(server, &uri_hello);
        httpd_register_uri_handler(server, &uri_index);
        httpd_register_uri_handler(server, &uri_time);
        return server;
    }

    printf("啟動伺服器失敗！\n");
    return NULL;
}