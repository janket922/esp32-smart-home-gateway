#include "App_Video_Streaming.h"
#include "App_Cloud_Upload.h"
#include "App_Sensor_Sync.h"

void app_main(void)
{
	esp_err_t nvs_ret = nvs_flash_init();
	if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		nvs_ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(nvs_ret);
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	Svc_Mqtt_Start();
	LED_Init();
	Svc_Video_Init();
	Key_Init();
	Svc_Isotp_Init();
    vTaskDelay(pdMS_TO_TICKS(3000));
	App_Video_Streaming_Init();
	App_Sensor_Sync_Init();
	App_Cloud_Upload_Init();
}
