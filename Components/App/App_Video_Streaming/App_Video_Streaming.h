#ifndef _APP_VIDEO_STREAMING_H
#define _APP_VIDEO_STREAMING_H

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/errno.h>
#include "esp_log.h"
#include "Svc_Video.h"
#include "LED.h"
#include "Key.h"
// 定义一个缓冲区池，用于存放采集的帧数据
#define VIDEO_POOL_SIZE 6
#define VIDEO_MAX_FRAME_SIZE 30 * 1024 // 每帧最大30KB
#define VIDEO_MAX_FRAME_RATE 25
#define VIDEO_TASK_DELAY_MS pdMS_TO_TICKS(1000/VIDEO_MAX_FRAME_RATE)

#define VIDEO_RESET_MAX_COUNT 5

#define APP_VIDEO_STREAMING_TASK_SIZE 20480
#define APP_VIDEO_STREAMING_TASK_PRIORITY 10
#define APP_VIDEO_STREAMING_SWITCH_TASK_SIZE 4096
#define APP_VIDEO_STREAMING_SWITCH_TASK_PRIORITY 5

typedef enum
{
	video_idle = 0,
	video_start,
	video_stop,
	video_shutdown,
	video_error,
} video_state;

void App_Video_Streaming_Init(void);
void App_Video_Streaming(void *arg);
void App_Video_Streaming_Switch(void *arg);

#endif //APP_VIDEO_STREAMING_H