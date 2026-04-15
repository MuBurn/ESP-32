#include "wifi.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// 运行时上下文：由连接函数创建并传递给事件回调。
typedef struct {
    EventGroupHandle_t event_group;
    int retry_count;
    int max_retry;
} wifi_runtime_t;

static const char *TAG = "wifi_station";

// WiFi/IP 事件统一回调：
// - STA_START: 触发首次连接
// - STA_DISCONNECTED: 在重试次数内自动重连
// - GOT_IP: 标记连接成功并打印 IP
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    wifi_runtime_t *runtime = (wifi_runtime_t *)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (runtime->retry_count < runtime->max_retry) {
            esp_wifi_connect();
            runtime->retry_count++;
            ESP_LOGW(TAG, "Retrying WiFi connect... (%d/%d)", runtime->retry_count, runtime->max_retry);
        } else {
            xEventGroupSetBits(runtime->event_group, WIFI_FAIL_BIT);
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        runtime->retry_count = 0;
        xEventGroupSetBits(runtime->event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

// 对外提供的 STA 连接入口。
// 该函数负责：参数校验 -> 网络栈准备 -> 事件注册 -> WiFi 配置与启动 -> 等待结果 -> 资源清理。
esp_err_t wifi_connect_sta(const char *ssid, const char *password, int max_retry)
{
    // 参数保护：避免无效 SSID 继续执行。
    if (ssid == NULL || strlen(ssid) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (password == NULL) {
        password = "";
    }

    if (max_retry < 0) {
        max_retry = WIFI_DEFAULT_MAXIMUM_RETRY;
    }

    // 初始化网络基础设施；已初始化时会返回 ESP_ERR_INVALID_STATE，可视为正常状态。
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    // 默认 STA netif 只创建一次，避免重复创建导致状态错误。
    static bool netif_created = false;
    if (!netif_created) {
        if (esp_netif_create_default_wifi_sta() == NULL) {
            return ESP_FAIL;
        }
        netif_created = true;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    wifi_runtime_t runtime = {
        .event_group = xEventGroupCreate(),
        .retry_count = 0,
        .max_retry = max_retry,
    };

    if (runtime.event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // 注册 WiFi 事件与 IP 事件，使用同一个回调处理。
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ret = esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        &runtime,
        &instance_any_id
    );
    if (ret != ESP_OK) {
        vEventGroupDelete(runtime.event_group);
        return ret;
    }

    ret = esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        &runtime,
        &instance_got_ip
    );
    if (ret != ESP_OK) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
        vEventGroupDelete(runtime.event_group);
        return ret;
    }

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    wifi_config.sta.sae_h2e_identifier[0] = '\0';

    // 按 STA 模式设置并启动 WiFi。
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip);
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
        vEventGroupDelete(runtime.event_group);
        return ret;
    }

    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip);
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
        vEventGroupDelete(runtime.event_group);
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_CONN) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip);
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
        vEventGroupDelete(runtime.event_group);
        return ret;
    }

    // 阻塞等待连接成功或失败事件。
    EventBits_t bits = xEventGroupWaitBits(
        runtime.event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY
    );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to SSID: %s", ssid);
        ret = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to connect to SSID: %s", ssid);
        ret = ESP_FAIL;
    }

    // 注销事件与释放资源，确保该接口可重复调用。
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
    vEventGroupDelete(runtime.event_group);

    return ret;
}
