#include "App_Cloud_Upload.h"

TaskHandle_t App_Cloud_Video_Frame_Upload_Task_Handle;
TaskHandle_t App_Cloud_Sensor_Data_Upload_Task_Handle;
void App_Cloud_Upload_Init(void)
{
	xTaskCreate(App_Cloud_Video_Frame_Upload, "App_Cloud_Video_Frame_Upload", APP_CLOUD_VIDEO_FRAME_UPLOAD_TASK_SIZE, NULL, APP_CLOUD_VIDEO_FRAME_UPLOAD_TASK_PRIORITY, &App_Cloud_Video_Frame_Upload_Task_Handle);
	xTaskCreate(App_Cloud_Sensor_Data_Upload, "App_Cloud_Sensor_Data_Upload", APP_CLOUDE_SENSOR_DATA_UPLOAD_TASK_SIZE, NULL, APP_CLOUDE_SENSOR_DATA_UPLOAD_TASK_PRIORITY, &App_Cloud_Sensor_Data_Upload_Task_Handle);
}

void App_Cloud_Video_Frame_Upload(void *arg)
{
	svc_frame_packet_t *frame_buf = NULL;
	unsigned char *encoded_buf = NULL;
	encoded_buf = (unsigned char *)heap_caps_malloc(MAX_ENCODER_LEN, MALLOC_CAP_SPIRAM);
	static const char *TAG = "App_Cloud_Video_Frame_Upload";
	if (encoded_buf == NULL)
	{
		ESP_LOGE(TAG, "Encoder buffer allocation failed");
		vTaskDelete(NULL);
		return;
	}
	while (1)
	{
		esp_err_t ret = Svc_Video_Frame_Hold(&frame_buf, portMAX_DELAY);
		if (ret != ESP_OK)
		{
			/* Video is closing or unavailable; wait for a later reopen. */
			vTaskDelay(pdMS_TO_TICKS(20));
			continue;
		}
		if (frame_buf != NULL && encoded_buf != NULL)
		{
			esp_err_t encode_ret = svc_encode_base64_with_jsonfilled(frame_buf, encoded_buf);
			// 发布编码后的数据
			if (encode_ret == ESP_OK)
			{
				Svc_Mqtt_Publish_For_VideoStream(USE_VIDEO_TOPIC_PUBLISH, (char *)encoded_buf);
			}
			else
			{
				ESP_LOGW(TAG, "Invalid frame or Base64 encoding failed; frame dropped");
			}
			ESP_ERROR_CHECK(Svc_Video_Frame_Release(&frame_buf));
		}
		else
		{
			ESP_LOGE(TAG, "Frame_buf or encoded_buf is NULL");
			return;
		}

		vTaskDelay(pdMS_TO_TICKS(APP_CLOUD_VIDEO_FRAME_UPLOAD_TASK_DELAY));
	}
}

void App_Cloud_Sensor_Data_Upload(void *arg)
{
	Sensor_data_t sensor_data = {0};
	char sensor_data_str[APP_CLOUDE_SENSOR_DATA_UPLOAD_MAX_LEN];
	while (1)
	{
		if (App_Sensor_Sync_Receive(&sensor_data) == ESP_OK)
		{
			int written = snprintf(sensor_data_str, sizeof(sensor_data_str), "{\"reported\": {\"temperature\": %.1f, \"humidity\": %.1f, \"illuminance\": %d}}", sensor_data.aht20_data.temperature, sensor_data.aht20_data.humidity, sensor_data.light);
			if (written > 0 && (size_t)written < sizeof(sensor_data_str))
			{
				Svc_Mqtt_Publish(USE_SENSOR_TOPIC_PUBLISH, sensor_data_str, written, 0, 0);
			}
		}
		else
		{
			ESP_LOGI("App_Cloud_Sensor_Data_Upload","No data to publish.\n");
		}
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}