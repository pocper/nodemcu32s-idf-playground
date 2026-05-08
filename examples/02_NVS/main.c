#include <stdio.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "nvs.h"
#include "nvs_flash.h"

void app_main() {
    // 1. 初始化NVS, Non-volatile Storage
    ESP_ERROR_CHECK(nvs_flash_init());

    // 2. 打開NVS命名空間
    nvs_handle_t nvs_handle;
    nvs_open("storage", NVS_READWRITE, &nvs_handle);

    // 3. 寫入
    int32_t value = 0;
    nvs_get_i32(nvs_handle, "value", &value);
    printf("Old value = %ld\n", value);
    value++;
    nvs_set_i32(nvs_handle, "value", value);

    // 4. 提交
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
}