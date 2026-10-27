#ifndef _WIFI_CONNECT_H
#define _WIFI_CONNECT_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_wifi.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
/**
 * @brief WiFi STA 配置参数
 */
typedef struct {
    char ssid[32];               // WiFi SSID
    char password[64];           // WiFi 密码
    uint8_t max_retry;           // 最大重试次数
    wifi_auth_mode_t auth_mode;  // 认证模式阈值，如 WIFI_AUTH_WPA2_PSK
    wifi_sae_pwe_method_t sae_pwe;      // WPA3 SAE PWE 模式，非 WPA3 可忽略
    const char *sae_h2e_id;      // WPA3 H2E 标识符，非 WPA3 可设为 ""
} wifi_sta_cfg_t;

/**
 * @brief 初始化并连接 WiFi（STA 模式）
 * @param config  WiFi 配置参数
 * @note  调用此函数前，必须已调用：
 *        1. nvs_flash_init()
 *        2. esp_netif_init()
 *        3. esp_event_loop_create_default()
 *        这些系统级初始化一般放在 app_main 开头，整个程序只执行一次。
 */
void wifi_sta_init(const wifi_sta_cfg_t *config);

#ifdef __cplusplus
}
#endif

#endif
