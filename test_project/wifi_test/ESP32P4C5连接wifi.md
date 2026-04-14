# ESP32P4C5连接wifi

[TOC]



#### ESP32P4C5连接WiFi的特点

ESP32-P4 本身没有内置 Wi-Fi，需通过 **ESP-Hosted 两芯片方案**，以 ESP32-C5 作为 Wi-Fi 协处理器（co-processor）来实现 Wi-Fi 功能。官方文档：https://github.com/espressif/esp-idf/blob/master/examples/wifi/README.md 

#### 为从机刷写程序(官方的esp32p4c6说是已经刷好了,建议还是刷写一下)

1. 从官网上下载slave工程文件(下载压缩包解压就行,官网给的代码在本地ESP-IDF由于不支持file协议的压缩包无法运行)https://github.com/espressif/esp-hosted-mcu/tree/main/slave

2. 配置相应目标芯片并烧录代码**(不要烧在p4上!从机有专门烧录的地方!)**

3. 配置传输层:一般主机和从机之间的通信时通过SPI或者SDIO 方式的要确保主机和从机的传输层完全一致(默认一般都是SDIO)![image-20260415003720183](C:\Users\13981\AppData\Roaming\Typora\typora-user-images\image-20260415003720183.png) SDIO官方文档:https://github.com/espressif/esp-hosted-mcu/blob/main/docs/sdio.md

4. 编译烧录:**注意**：如果烧录失败，可能是 ESP32-P4（host）阻止了烧录。请先将 host 置于 bootloader 模式(在终端中运行)：

   ```c
   esptool.py -p <host_serial_port> --before default_reset --after no_reset run
   ```

   

#### 让一个新建工程连上wifi

##### 一.添加依赖组件

在 `main/CMakeLists.txt` 的 `idf_component_register` 中添加 (不加也行)：

```cmake
REQUIRES esp_wifi esp_netif esp_event nvs_flash
```

同时在 `idf_component.yml` 中添加 ESP-Hosted 和 Wi-Fi Remote 组件：

```cmake
idf.py add-dependency "espressif/esp_wifi_remote"
idf.py add-dependency "espressif/esp_hosted"
```

**建议用图形化界面直接添加**（打开vscode命令行，选择乐鑫组件注册表，直接搜索就行）

![image-20260414230659532](C:\Users\13981\AppData\Roaming\Typora\typora-user-images\image-20260414230659532.png)

![image-20260414230541355](C:\Users\13981\AppData\Roaming\Typora\typora-user-images\image-20260414230541355.png)

##### 二.menuconfig 配置(重点)

首先要开启ESP-Hosted(这个组件的位置相当阴间![image-20260414235115909](C:\Users\13981\AppData\Roaming\Typora\typora-user-images\image-20260414235115909.png)居然在eppp_link里启用的,建议直接搜索) 与ESP-wifi-remote 组件

![image-20260414234424554](C:\Users\13981\AppData\Roaming\Typora\typora-user-images\image-20260414234424554.png)

![image-20260414231611718](C:\Users\13981\AppData\Roaming\Typora\typora-user-images\image-20260414231611718.png)

不要开**Host wifi** **!!!**在 ESP32-P4 + ESP32-C5 的两芯片方案中，**Host 端的原生 Wi-Fi 需要禁用**。

虽然ESP32-P4本身没有内置wifi但是如果你打开了Host wifi 在你调用wifi-remote 组件中的函数时会**导致冲突**!

在wifi-remote中选择对应从机芯片的**slave target**

#### 连接wifi 的有关代码

```c
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

#define WIFI_SSID           "your_ssid"
#define WIFI_PASS           "your_password"
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

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
```

