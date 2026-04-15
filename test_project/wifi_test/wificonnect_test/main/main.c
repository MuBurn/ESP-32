#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "wifi.h"

#define WIFI_SSID      "Mburn"
#define WIFI_PASS      "MouCMY0728"

static const char *TAG = "app_main";

// 应用入口：
// 1) 完成 NVS 初始化（WiFi 依赖）
// 2) 调用可复用的 WiFi 模块进行 STA 连接
void app_main(void)
{
    // 初始化 NVS（必须在 Wi-Fi 之前）
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 连接逻辑已经下沉到 wifi.c，main.c 只保留参数和调用。
    ret = wifi_connect_sta(WIFI_SSID, WIFI_PASS, WIFI_DEFAULT_MAXIMUM_RETRY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect failed: %s", esp_err_to_name(ret));
    }
}