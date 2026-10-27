#ifndef _APP_SENSOR_SYNC_H_
#define _APP_SENSOR_SYNC_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "Svc_Isotp.h"

#define APP_SENSOR_SYNC_TASK_SIZE 4096
#define APP_SENSOR_SYNC_TASK_PRIORITY 10
#define APP_SENSOR_SYNC_QUEUE_SIZE 12

void App_Sensor_Sync_Init(void);
void App_Sensor_Sync(void *arg);
void App_Sensor_Sync_Send(Sensor_data_t *pdata);
esp_err_t App_Sensor_Sync_Receive(Sensor_data_t *pdata);
#endif
