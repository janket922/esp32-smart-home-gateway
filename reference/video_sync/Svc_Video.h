#ifndef SVC_VIDEO_H
#define SVC_VIDEO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SVC_VIDEO_POOL_SIZE       6
#define SVC_VIDEO_MAX_FRAME_SIZE  (30 * 1024)

typedef enum { video_off = 0, video_on } svc_video_state_t;
typedef struct {
    uint8_t *payload;
    size_t length;
} svc_frame_packet_t;

/* First Init must run in app_main before creating video/upload tasks.
 * One capture producer, one upload consumer. Returned frames belong to the
 * task that called Hold, until that same task calls Release. */
esp_err_t Svc_Video_Init(void);
esp_err_t Svc_Video_Deinit(void);
esp_err_t Svc_Video_Reset(void);
esp_err_t Svc_Video_Ctrl(svc_video_state_t state);
esp_err_t Svc_Video_Start(void);
esp_err_t Svc_Video_Stop(void);
esp_err_t Svc_Video_Frame_Ready(void);
esp_err_t Svc_Video_Frame_Hold(svc_frame_packet_t **p, TickType_t wait);
esp_err_t Svc_Video_Frame_Release(svc_frame_packet_t **p);

#endif
