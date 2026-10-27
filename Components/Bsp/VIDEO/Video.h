#ifndef _BSP_VIDEO_H_
#define _BSP_VIDEO_H_

#include "linux/videodev2.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "esp_video_ioctl.h"
#include "esp_cam_sensor_xclk.h"
#include "sdkconfig.h"
#include "esp_check.h"
#include "XL9555.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/errno.h>


#include "esp_log.h"
/**
 * @brief DVP camera sensor common configuration
 */
#if CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
#define BSP_ENABLE_DVP_CAM_SENSOR                   1
#define BSP_DVP_SCCB_I2C_PORT                       CONFIG_EXAMPLE_DVP_SCCB_I2C_PORT
#define BSP_DVP_SCCB_I2C_SCL_PIN                    CONFIG_EXAMPLE_DVP_SCCB_I2C_SCL_PIN
#define BSP_DVP_SCCB_I2C_SDA_PIN                    CONFIG_EXAMPLE_DVP_SCCB_I2C_SDA_PIN
#define BSP_DVP_CAM_SENSOR_RESET_PIN                CONFIG_EXAMPLE_DVP_CAM_SENSOR_RESET_PIN
#define BSP_DVP_CAM_SENSOR_PWDN_PIN                 CONFIG_EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN
#define BSP_DVP_SCCB_I2C_FREQ                       CONFIG_EXAMPLE_DVP_SCCB_I2C_FREQ
/**
 * @brief DVP clock frequency configuration
 */
#if (defined(BSP_DVP_XCLK_PIN) && BSP_DVP_XCLK_PIN >= 0)
#define BSP_DVP_XCLK_FREQ                           CONFIG_EXAMPLE_DVP_XCLK_FREQ
#else
#define BSP_DVP_XCLK_FREQ                           0
#endif /* BSP_DVP_XCLK_PIN >= 0 */
#endif /* CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR */
/**
 * @brief DVP interface configuration
 */
#define BSP_DVP_XCLK_PIN                            -1
#define BSP_DVP_PCLK_PIN                            CONFIG_EXAMPLE_DVP_PCLK_PIN
#define BSP_DVP_VSYNC_PIN                           CONFIG_EXAMPLE_DVP_VSYNC_PIN
#define BSP_DVP_DE_PIN                              CONFIG_EXAMPLE_DVP_DE_PIN
#define BSP_DVP_D0_PIN                              CONFIG_EXAMPLE_DVP_D0_PIN
#define BSP_DVP_D1_PIN                              CONFIG_EXAMPLE_DVP_D1_PIN
#define BSP_DVP_D2_PIN                              CONFIG_EXAMPLE_DVP_D2_PIN
#define BSP_DVP_D3_PIN                              CONFIG_EXAMPLE_DVP_D3_PIN
#define BSP_DVP_D4_PIN                              CONFIG_EXAMPLE_DVP_D4_PIN
#define BSP_DVP_D5_PIN                              CONFIG_EXAMPLE_DVP_D5_PIN
#define BSP_DVP_D6_PIN                              CONFIG_EXAMPLE_DVP_D6_PIN
#define BSP_DVP_D7_PIN                              CONFIG_EXAMPLE_DVP_D7_PIN

//V4L2
#define ESP_VIDEO_DVP_DEVICE_NAME           "/dev/video2"
#define BSP_VIDEO_MAX_FRAME_SIZE 30 * 1024 // 每帧最大30KB


typedef struct {
    uint8_t *payload; // 指向 PSRAM 中的图像数据
    size_t length;    // 帧大小
} bsp_frame_packet_t;

esp_err_t bsp_video_init();
esp_err_t bsp_video_deinit();
esp_err_t bsp_video_ready(void);
esp_err_t bsp_video_start(void);
esp_err_t bsp_video_get_frame(bsp_frame_packet_t *p);
esp_err_t bsp_video_stop(void);
esp_err_t bsp_video_off(void);

#endif
