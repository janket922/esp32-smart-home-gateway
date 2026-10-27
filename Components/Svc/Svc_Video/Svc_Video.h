#ifndef _SVC_VIDEO_H_
#define _SVC_VIDEO_H_



#include <stdint.h>

#include "Video.h"

// 定义一个缓冲区池，用于存放采集的帧数据
#define SVC_VIDEO_POOL_SIZE 6
#define SVC_VIDEO_MAX_FRAME_SIZE 30 * 1024 // 每帧最大30KB

typedef enum {
   video_off = 0,
   video_on,
}svc_video_state_t;

typedef struct {
    uint8_t *payload; // 指向 PSRAM 中的图像数据
    size_t length;    // 帧大小
}svc_frame_packet_t;

void Svc_Video_Init(void);
void Svc_Video_Deinit(void);
void Svc_Video_Reset(void);
esp_err_t Svc_Video_Start(void);
esp_err_t Svc_Video_Stop(void);
esp_err_t Svc_Video_Frame_Ready(void);
esp_err_t Svc_Video_Frame_Hold(svc_frame_packet_t **p, TickType_t xTicksToWait);
esp_err_t Svc_Video_Frame_Release(svc_frame_packet_t **p);
void Svc_Buf_pool_Init(void);
void Svc_Buf_pool_Deinit(void);
esp_err_t Svc_Video_Ctrl(svc_video_state_t state);

#endif
