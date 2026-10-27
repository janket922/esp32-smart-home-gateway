#include "App_Sensor_Sync.h"

static TaskHandle_t App_Sensor_Sync_Task_Handle;
static QueueHandle_t App_Sensor_Sync_Queue_Handle;
static const char *TAG = "App_Sensor_Sync";
void App_Sensor_Sync_Init(void)
{
	App_Sensor_Sync_Queue_Handle = xQueueCreate(APP_SENSOR_SYNC_QUEUE_SIZE, sizeof(Sensor_data_t));
	if (App_Sensor_Sync_Queue_Handle == NULL)
	{
		ESP_LOGE(TAG,"Queue create fail!");
	}
	if (xTaskCreate(App_Sensor_Sync, "App_Sensor_Sync", APP_SENSOR_SYNC_TASK_SIZE, NULL, APP_SENSOR_SYNC_TASK_PRIORITY, &App_Sensor_Sync_Task_Handle) != pdPASS)
	{
		ESP_LOGE(TAG,"Task create fail!");
	}
}

void App_Sensor_Sync(void *arg)
{
	uint8_t rx_buffer[sizeof(Sensor_data_t)] = {0};
	uint32_t rx_size = 0;
	while (1)
	{
		esp_err_t ret = Svc_Isotp_Poll();
		if (ret != ESP_OK)
		{
			ESP_LOGE(TAG, "Svc_Isotp_Poll failed: %s", esp_err_to_name(ret));
		}
		else
		{
			ret = Svc_Isotp_Receive(rx_buffer,
									sizeof(rx_buffer),
									&rx_size);
			if (ret == ESP_OK)
			{
				if (rx_size == sizeof(Sensor_data_t))
				{
					App_Sensor_Sync_Send((Sensor_data_t *)rx_buffer);
				}
				ESP_LOGI(TAG, "Received %d bytes", rx_size);
			}
			else if (ret != ESP_FAIL && ret != ESP_ERR_NOT_FOUND)
			{
				ESP_LOGE("ISOTP", "Receive failed: %s",
						 esp_err_to_name(ret));
			}
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

void App_Sensor_Sync_Send(Sensor_data_t *pdata)
{
	if (App_Sensor_Sync_Queue_Handle != NULL && pdata != NULL)
	{
		xQueueSend(App_Sensor_Sync_Queue_Handle, pdata, 0);
	}
	else
	{
		ESP_LOGE(TAG, "App_Sensor_Sync_Queue_Handle is NULL or pdata is NULL");
	}
}

esp_err_t App_Sensor_Sync_Receive(Sensor_data_t *pdata)
{
	if (App_Sensor_Sync_Queue_Handle != NULL && pdata != NULL)
	{
		if (xQueueReceive(App_Sensor_Sync_Queue_Handle, pdata, 0) != pdTRUE)
		{
			return ESP_FAIL;
		}
	}
	else
	{
		ESP_LOGE(TAG, "App_Sensor_Sync_Queue_Handle is NULL or pdata is NULL");
		return ESP_FAIL;
	}
	return ESP_OK;
}