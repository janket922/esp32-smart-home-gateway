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
    const char *TAG = "video_upload";
    svc_frame_packet_t *frame_buf = NULL;
    unsigned char *encoded_buf = heap_caps_malloc(MAX_ENCODER_LEN, MALLOC_CAP_SPIRAM);
    if (encoded_buf == NULL) {
        ESP_LOGE(TAG, "Cannot allocate encoder buffer");
        vTaskDelete(NULL);
        return;
    }
    for (;;) {
        esp_err_t ret = Svc_Video_Frame_Hold(&frame_buf, pdMS_TO_TICKS(100));
        if (ret != ESP_OK) {
            /* Closed or no new frame: retry later, never use an old pointer. */
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        /* These are the existing encoder/MQTT calls. Their separate bounds
         * and publish-error handling are not changed by the synchronization fix. */
        svc_encode_base64_with_jsonfilled(frame_buf, encoded_buf);
        int published = Svc_Mqtt_Publish_For_VideoStream(USE_VIDEO_TOPIC_PUBLISH,
                                                       (char *)encoded_buf);
        /* Even a failed publish must end our use of this frame. */
        ESP_ERROR_CHECK(Svc_Video_Frame_Release(&frame_buf));
        if (published < 0) ESP_LOGW(TAG, "Video publish failed");
        vTaskDelay(APP_CLOUD_VIDEO_FRAME_UPLOAD_TASK_DELAY);
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
			sprintf(sensor_data_str, "{\"reported\": {\"temperature\": %.1f, \"humidity\": %.1f, \"illuminance\": %d}}", sensor_data.aht20_data.temperature, sensor_data.aht20_data.humidity, sensor_data.light);
			Svc_Mqtt_Publish(USE_SENSOR_TOPIC_PUBLISH, sensor_data_str, strlen(sensor_data_str), 0, 0);
		}
		else
		{
			ESP_LOGI("App_Cloud_Sensor_Data_Upload","No data to publish.\n");
		}
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}