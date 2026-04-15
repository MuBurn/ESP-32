#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 默认重试次数：当 max_retry 传入负数时使用该值。
#define WIFI_DEFAULT_MAXIMUM_RETRY 3

// 以 STA 模式连接到指定 AP。
// - ssid: 目标热点名称，不能为空。
// - password: 热点密码，可为空字符串。
// - max_retry: 断线后最大重连次数；< 0 时使用 WIFI_DEFAULT_MAXIMUM_RETRY。
// 返回值：ESP_OK 表示连接成功，其他错误码表示初始化/配置/连接过程失败。
esp_err_t wifi_connect_sta(const char *ssid, const char *password, int max_retry);

#ifdef __cplusplus
}
#endif
