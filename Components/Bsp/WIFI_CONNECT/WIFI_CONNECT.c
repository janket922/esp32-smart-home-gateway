#include "WIFI_CONNECT.h"

static const char *TAG = "Bsp_Wifi:";

/* FreeRTOS 事件组：通知连接状态 */
EventGroupHandle_t s_wifi_event_group;

static int s_retry_num = 0;
/* Event handlers outlive the caller stack. */
static wifi_sta_cfg_t s_wifi_config;

static void event_handler(void *arg, esp_event_base_t event_base,
						  int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
		esp_wifi_connect();
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
		wifi_sta_cfg_t *cfg = (wifi_sta_cfg_t *)arg;
		if (s_retry_num < cfg->max_retry)
		{
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
		}
		else
		{
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		}
		ESP_LOGI(TAG, "connect to the AP fail");
	}
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
	{
		ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

void wifi_sta_init(const wifi_sta_cfg_t *config)
{
	if (config == NULL || s_wifi_event_group != NULL) return;
	s_wifi_config = *config;
	s_wifi_event_group = xEventGroupCreate();
	if (s_wifi_event_group == NULL)
	{
		ESP_LOGE(TAG, "Wi-Fi event group allocation failed");
		return;
	}

	// 创建默认 WiFi STA 接口
	esp_netif_create_default_wifi_sta();

	// WiFi 初始化
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	// 注册事件回调（将 config 指针作为用户参数传入，方便在回调中获取重试次数）
	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
														ESP_EVENT_ANY_ID,
														&event_handler,
														(void *)&s_wifi_config,
														&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
														IP_EVENT_STA_GOT_IP,
														&event_handler,
														(void *)&s_wifi_config,
														&instance_got_ip));

	// 配置 WiFi 参数
	wifi_config_t wifi_config = {
		.sta = {
			.threshold.authmode = config->auth_mode,
			.sae_pwe_h2e = config->sae_pwe,
		},
	};
	strncpy((char *)wifi_config.sta.ssid, config->ssid, sizeof(wifi_config.sta.ssid));
	strncpy((char *)wifi_config.sta.password, config->password, sizeof(wifi_config.sta.password));
	if (config->sae_h2e_id)
	{
		strncpy((char *)wifi_config.sta.sae_h2e_identifier, config->sae_h2e_id,
				sizeof(wifi_config.sta.sae_h2e_identifier) - 1);
	}

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "wifi_sta_init finished.");

	// 等待连接成功或失败
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
										   WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
										   pdFALSE,
										   pdFALSE,
										   portMAX_DELAY);

	if (bits & WIFI_CONNECTED_BIT)
	{
		ESP_LOGI(TAG, "connected to ap SSID:%s", config->ssid);
	}
	else if (bits & WIFI_FAIL_BIT)
	{
		ESP_LOGI(TAG, "Failed to connect to SSID:%s", config->ssid);
	}
	else
	{
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}
}