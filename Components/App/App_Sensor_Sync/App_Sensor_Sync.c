#include "App_Sensor_Sync.h"

static TaskHandle_t App_Sensor_Sync_Task_Handle;
static QueueHandle_t App_Sensor_Sync_Queue_Handle;
static const char *TAG = "App_Sensor_Sync";

void App_Sensor_Sync_Init(void)
{
    if (App_Sensor_Sync_Queue_Handle != NULL) return;
    App_Sensor_Sync_Queue_Handle = xQueueCreate(APP_SENSOR_SYNC_QUEUE_SIZE, sizeof(Sensor_data_t));
    if (App_Sensor_Sync_Queue_Handle == NULL)
    {
        ESP_LOGE(TAG, "Queue create failed");
        return;
    }
    if (xTaskCreate(App_Sensor_Sync, "App_Sensor_Sync", APP_SENSOR_SYNC_TASK_SIZE, NULL,
                    APP_SENSOR_SYNC_TASK_PRIORITY, &App_Sensor_Sync_Task_Handle) != pdPASS)
    {
        ESP_LOGE(TAG, "Task create failed");
        vQueueDelete(App_Sensor_Sync_Queue_Handle);
        App_Sensor_Sync_Queue_Handle = NULL;
    }
}

void App_Sensor_Sync(void *arg)
{
    (void)arg;
    uint8_t rx_buffer[ISOTP_RECV_BUF_SIZE];
    TickType_t last_diag = xTaskGetTickCount();
    while (1)
    {
        esp_err_t ret = Svc_Isotp_Poll();
        if (ret == ESP_OK)
        {
            uint32_t rx_size = 0;
            ret = Svc_Isotp_Receive(rx_buffer, sizeof(rx_buffer), &rx_size);
            if (ret == ESP_OK)
            {
                if (rx_size == sizeof(Sensor_data_t))
                {
                    Sensor_data_t data;
                    memcpy(&data, rx_buffer, sizeof(data));
                    App_Sensor_Sync_Send(&data);
                }
                else
                {
                    ESP_LOGW(TAG, "Unexpected sensor length: %lu, expected %u",
                             (unsigned long)rx_size, (unsigned)sizeof(Sensor_data_t));
                }
            }
            else if (ret != ESP_ERR_NOT_FOUND && ret != ESP_FAIL)
            {
                ESP_LOGW(TAG, "ISO-TP receive failed: %s", esp_err_to_name(ret));
            }
        }
        TickType_t now = xTaskGetTickCount();
        if ((TickType_t)(now - last_diag) >= pdMS_TO_TICKS(1000))
        {
            if (ret != ESP_OK && ret != ESP_ERR_NOT_FOUND && ret != ESP_FAIL)
                ESP_LOGW(TAG, "ISO-TP status: %s", esp_err_to_name(ret));
            Svc_Isotp_Log_Diagnostics();
            last_diag = now;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void App_Sensor_Sync_Send(Sensor_data_t *pdata)
{
    if (App_Sensor_Sync_Queue_Handle == NULL || pdata == NULL)
    {
        ESP_LOGE(TAG, "Sensor queue or input unavailable");
        return;
    }
    /* Never stall ISO-TP polling behind the much slower MQTT consumer. */
    if (xQueueSend(App_Sensor_Sync_Queue_Handle, pdata, 0) != pdTRUE)
    {
        static TickType_t last_full_log;
        TickType_t now = xTaskGetTickCount();
        if ((TickType_t)(now - last_full_log) >= pdMS_TO_TICKS(1000))
        {
            ESP_LOGW(TAG, "Sensor upload queue full; incoming sample dropped");
            last_full_log = now;
        }
    }
}

esp_err_t App_Sensor_Sync_Receive(Sensor_data_t *pdata)
{
    if (pdata == NULL) return ESP_ERR_INVALID_ARG;
    if (App_Sensor_Sync_Queue_Handle == NULL) return ESP_ERR_INVALID_STATE;
    return xQueueReceive(App_Sensor_Sync_Queue_Handle, pdata, pdMS_TO_TICKS(100)) == pdTRUE
        ? ESP_OK : ESP_ERR_NOT_FOUND;
}
