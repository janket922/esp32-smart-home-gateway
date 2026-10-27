#include "App_Video_Streaming.h"

TaskHandle_t App_Video_Streaming_Task_Handle;
TaskHandle_t App_Video_Streaming_Switch_Handle;

video_state current_video_state = video_idle;

uint8_t get_frame_retry_count = 0;

void App_Video_Streaming_Init(void)
{
	xTaskCreate(App_Video_Streaming, "App_Video_Streaming", APP_VIDEO_STREAMING_TASK_SIZE, NULL, APP_VIDEO_STREAMING_TASK_PRIORITY, &App_Video_Streaming_Task_Handle);
	xTaskCreate(App_Video_Streaming_Switch, "App_Video_Streaming_Switch", APP_VIDEO_STREAMING_SWITCH_TASK_SIZE, NULL, APP_VIDEO_STREAMING_SWITCH_TASK_PRIORITY, &App_Video_Streaming_Switch_Handle);
}

void App_Video_Streaming(void *arg)
{
	const char *TAG = "App_Video_Streaming";
	while (1)
	{
		switch (current_video_state)
		{
		case video_idle:
			if (Svc_Video_Ctrl(video_on) != ESP_OK)
			{
				LED(0);
				ESP_LOGE(TAG, "Fail to open video");
			}
			else
			{
				LED_TOGGER();
			}
			vTaskDelay(pdMS_TO_TICKS(1000));
			break;
		case video_start:
			if (Svc_Video_Start() != ESP_OK)
			{
				ESP_LOGE(TAG, "Fail to start video");
			}
			else
			{
				LED_TOGGER();
				if (get_frame_retry_count > 20)
				{
					current_video_state = video_error;
					break;
				}
				if (Svc_Video_Frame_Ready() != ESP_OK)
				{
					get_frame_retry_count++;
				}
				else
				{
					get_frame_retry_count = 0;
				}
			}
			vTaskDelay(VIDEO_TASK_DELAY_MS);
			break;
		case video_stop:
			if (Svc_Video_Stop() != ESP_OK)
			{
				ESP_LOGE(TAG, "Fail to stop video");
			}
			LED(1);
			vTaskDelay(pdMS_TO_TICKS(2000));
			break;
		case video_error:
			static uint8_t reset_count = 0;
			if (reset_count >= VIDEO_RESET_MAX_COUNT)
			{
				current_video_state = video_shutdown;
			}
			else
			{
				if (Svc_Video_Stop() == ESP_OK)
				{
					Svc_Video_Reset();
					current_video_state = video_idle;
				}
				else
				{
					ESP_LOGE(TAG, "Error and cannot reset");
				}
			}
			reset_count++;
			LED_TOGGER();
			vTaskDelay(pdMS_TO_TICKS(2000));
			break;
		case video_shutdown:
			Svc_Video_Deinit();
			LED(0);
			vTaskDelay(pdMS_TO_TICKS(5000));
			break;
		}
	}
}

void App_Video_Streaming_Switch(void *arg)
{
	while (1)
	{
		key_state key1_state = Key_Get_State(KEY_1);
		key_state key2_state = Key_Get_State(KEY_2);
		static uint8_t key1_count = 1;
		if (key1_state == KEY1_PRESSED)
		{
			if (key1_count % 2 == 0)
			{
				current_video_state = video_stop;
			}
			else
			{
				current_video_state = video_start;
			}
			key1_count++;
		}
		if (key2_state == KEY2_PRESSED)
		{
			Svc_Video_Init();
			current_video_state = video_idle;
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}