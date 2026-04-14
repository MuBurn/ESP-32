#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#define WIFI_SSID      "Mburn"
#define WIFI_PASS      "MouCMY0728"
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi_station";
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

/**
 * @brief WiFi和IP事件处理函数
 * 
 * 该函数用于处理WiFi连接过程中的各种事件：
 * - WIFI_EVENT_STA_START：WiFi启动，尝试连接AP
 * - WIFI_EVENT_STA_DISCONNECTED：WiFi断开，重试3次，失败则设置失败位
 * - IP_EVENT_STA_GOT_IP：成功获取IP，重置重试次数，设置连接成功位
 * 
 * @param arg         用户参数（未使用）
 * @param event_base  事件基类型（WIFI_EVENT或IP_EVENT）
 * @param event_id    事件ID
 * @param event_data  事件数据
 */
static void event_handler(void* arg, esp_event_base_t event_base,
                           int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 3) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Retrying to connect to WiFi...");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief 初始化WiFi为STA模式并连接指定AP
 * 
 * 该函数完成WiFi连接的全部流程：
 * 1. 初始化TCP/IP协议栈
 * 2. 创建事件循环
 * 3. 创建STA网络接口
 * 4. 初始化WiFi驱动
 * 5. 注册WiFi和IP事件处理器
 * 6. 配置WiFi参数（SSID、密码等）
 * 7. 启动WiFi
 * 8. 阻塞等待连接结果（成功或失败）
 */
void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    // 1. 初始化 TCP/IP 协议栈
    ESP_ERROR_CHECK(esp_netif_init());
    // 2. 创建默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // 3. 创建默认 STA 网络接口
    esp_netif_create_default_wifi_sta();

    // 4. 初始化 Wi-Fi 驱动
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 5. 注册事件处理函数
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
            ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
            IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    // 6. 配置 Wi-Fi 参数
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            .sae_h2e_identifier = "",
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // 7. 启动 Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start());

    // 8. 等待连接结果
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to SSID: %s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to SSID: %s", WIFI_SSID);
    }
}

/**
 * @brief 应用程序主入口
 * 
 * 1. 初始化NVS（非易失性存储），为WiFi配置做准备
 * 2. 调用wifi_init_sta()完成WiFi连接
 */
void app_main(void)
{
    // 初始化 NVS（必须在 Wi-Fi 之前）
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();
}