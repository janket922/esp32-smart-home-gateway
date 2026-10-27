#ifndef _App_Cloud_Upload_H_
#define _App_Cloud_Upload_H_

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Svc_Mqtt.h"
#include "Svc_Video.h"
#include "Svc_Isotp.h"
#include "Svc_Encode_Base64.h"
#include "App_Video_Streaming.h"
#include "App_Sensor_Sync.h"
#define APP_CLOUD_VIDEO_FRAME_UPLOAD_TASK_SIZE 20480
#define APP_CLOUD_VIDEO_FRAME_UPLOAD_TASK_PRIORITY 10
#define APP_CLOUD_VIDEO_FRAME_UPLOAD_TASK_DELAY VIDEO_TASK_DELAY_MS
#define APP_CLOUDE_SENSOR_DATA_UPLOAD_TASK_SIZE 4096
#define APP_CLOUDE_SENSOR_DATA_UPLOAD_TASK_PRIORITY 5
#define APP_CLOUDE_SENSOR_DATA_UPLOAD_TASK_DELAY 1000

#define APP_CLOUDE_SENSOR_DATA_UPLOAD_MAX_LEN 200
void App_Cloud_Upload_Init(void);
void App_Cloud_Video_Frame_Upload(void *arg);
void App_Cloud_Sensor_Data_Upload(void *arg);
#endif