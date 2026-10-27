/*
 * SPDX-FileCopyrightText: 2025 乐鑫科技（上海）有限公司
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "linux/videodev2.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "esp_video_ioctl.h"
#include "example_video_common_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SCCB(I2C)预初始化端口配置
 */
#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#define EXAMPLE_SCCB_I2C_PORT_INIT_BY_APP              CONFIG_EXAMPLE_SCCB_I2C_PORT_INIT_BY_APP
#endif /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */

/**
 * @brief MIPI-CSI摄像头传感器通用配置
 */
#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
#define EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR              1
#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#define EXAMPLE_MIPI_CSI_SCCB_I2C_PORT                  EXAMPLE_SCCB_I2C_PORT_INIT_BY_APP
#else /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
#define EXAMPLE_MIPI_CSI_SCCB_I2C_PORT                  CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT
#endif /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */

#define EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ                  CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ

#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR
#define EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR              1
#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#define EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_PORT       EXAMPLE_SCCB_I2C_PORT_INIT_BY_APP
#else /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
#define EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_PORT       CONFIG_EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_PORT
#endif /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */

#define EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_FREQ       CONFIG_EXAMPLE_MIPI_CSI_CAM_MOTOR_SCCB_I2C_FREQ
#endif /* CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_MOTOR */
#endif /* CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR */

/**
 * @brief DVP摄像头传感器通用配置
 */
#if CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
#define EXAMPLE_ENABLE_DVP_CAM_SENSOR                   1
#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#define EXAMPLE_DVP_SCCB_I2C_PORT                       EXAMPLE_SCCB_I2C_PORT_INIT_BY_APP
#else /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
#define EXAMPLE_DVP_SCCB_I2C_PORT                       CONFIG_EXAMPLE_DVP_SCCB_I2C_PORT
#endif /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */


/**
 * @brief DVP时钟频率配置
 */
#if (defined(EXAMPLE_DVP_XCLK_PIN) && EXAMPLE_DVP_XCLK_PIN >= 0)
#define EXAMPLE_DVP_XCLK_FREQ                           CONFIG_EXAMPLE_DVP_XCLK_FREQ
#else
#define EXAMPLE_DVP_XCLK_FREQ                           0
#endif /* EXAMPLE_DVP_XCLK_PIN >= 0 */
#endif /* CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR */

/**
 * @brief SPI摄像头传感器通用配置
 */
#if CONFIG_EXAMPLE_ENABLE_SPI_CAM_0_SENSOR
#if CONFIG_EXAMPLE_SPI_CAM_0_INTERFACE_SPI
#define EXAMPLE_SPI_CAM_0_INTERFACE                     ESP_CAM_CTLR_SPI_CAM_INTF_SPI
#elif CONFIG_EXAMPLE_SPI_CAM_0_INTERFACE_PARLIO
#define EXAMPLE_SPI_CAM_0_INTERFACE                     ESP_CAM_CTLR_SPI_CAM_INTF_PARLIO
#endif /* CONFIG_EXAMPLE_SPI_CAM_0_INTERFACE_SPI */

#if CONFIG_EXAMPLE_SPI_CAM_0_IO_MODE_4BIT
#define EXAMPLE_SPI_CAM_0_IO_MODE                       ESP_CAM_CTLR_SPI_CAM_IO_MODE_4BIT
#elif CONFIG_EXAMPLE_SPI_CAM_0_IO_MODE_2BIT
#define EXAMPLE_SPI_CAM_0_IO_MODE                       ESP_CAM_CTLR_SPI_CAM_IO_MODE_2BIT
#else /* CONFIG_EXAMPLE_SPI_CAM_0_IO_MODE_2BIT */
#define EXAMPLE_SPI_CAM_0_IO_MODE                       ESP_CAM_CTLR_SPI_CAM_IO_MODE_1BIT
#endif /* CONFIG_EXAMPLE_SPI_CAM_0_IO_MODE_4BIT */

#define EXAMPLE_ENABLE_SPI_CAM_SENSOR                   1
#define EXAMPLE_ENABLE_SPI_CAM_0_SENSOR                 1
#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#define EXAMPLE_SPI_CAM_0_SCCB_I2C_PORT                 EXAMPLE_SCCB_I2C_PORT_INIT_BY_APP
#else /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
#define EXAMPLE_SPI_CAM_0_SCCB_I2C_PORT                 CONFIG_EXAMPLE_SPI_CAM_0_SCCB_I2C_PORT
#endif /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
#define EXAMPLE_SPI_CAM_0_SCCB_I2C_FREQ                 CONFIG_EXAMPLE_SPI_CAM_0_SCCB_I2C_FREQ

/**
 * @brief SPI摄像头传感器SPI端口配置
 */
#define EXAMPLE_SPI_CAM_0_SPI_PORT                      CONFIG_EXAMPLE_SPI_CAM_0_SPI_PORT

/**
 * @brief SPI摄像头传感器时钟资源配置
 */
#if CONFIG_EXAMPLE_SPI_CAM_0_XCLK_USE_LEDC
#define EXAMPLE_SPI_CAM_0_XCLK_RESOURCE                 ESP_CAM_SENSOR_XCLK_LEDC
#elif CONFIG_EXAMPLE_SPI_CAM_0_XCLK_USE_CLOCK_ROUTER
#define EXAMPLE_SPI_CAM_0_XCLK_RESOURCE                 ESP_CAM_SENSOR_XCLK_ESP_CLOCK_ROUTER
#endif /* CONFIG_EXAMPLE_SPI_CAM_0_XCLK_USE_LEDC */

#if defined(EXAMPLE_SPI_CAM_0_XCLK_PIN) && EXAMPLE_SPI_CAM_0_XCLK_PIN >= 0
#define EXAMPLE_SPI_CAM_0_XCLK_FREQ                     CONFIG_EXAMPLE_SPI_CAM_0_XCLK_FREQ
#else
#define EXAMPLE_SPI_CAM_0_XCLK_FREQ                     0
#endif /* EXAMPLE_SPI_CAM_0_XCLK_PIN >= 0 */

#if CONFIG_EXAMPLE_SPI_CAM_0_XCLK_USE_LEDC
#define EXAMPLE_SPI_CAM_0_XCLK_TIMER                    CONFIG_EXAMPLE_SPI_CAM_0_XCLK_TIMER
#define EXAMPLE_SPI_CAM_0_XCLK_TIMER_CHANNEL            CONFIG_EXAMPLE_SPI_CAM_0_XCLK_TIMER_CHANNEL
#endif /* CONFIG_EXAMPLE_SPI_CAM_XCLK_USE_LEDC */

#if CONFIG_EXAMPLE_ENABLE_SPI_CAM_1_SENSOR
#define EXAMPLE_ENABLE_SPI_CAM_1_SENSOR                 1
#if CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP
#define EXAMPLE_SPI_CAM_1_SCCB_I2C_PORT                 EXAMPLE_SCCB_I2C_PORT_INIT_BY_APP
#else /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
#define EXAMPLE_SPI_CAM_1_SCCB_I2C_PORT                 CONFIG_EXAMPLE_SPI_CAM_1_SCCB_I2C_PORT
#endif /* CONFIG_EXAMPLE_SCCB_I2C_INIT_BY_APP */
#define EXAMPLE_SPI_CAM_1_SCCB_I2C_FREQ                 CONFIG_EXAMPLE_SPI_CAM_1_SCCB_I2C_FREQ

#define EXAMPLE_SPI_CAM_1_SPI_PORT                      CONFIG_EXAMPLE_SPI_CAM_1_SPI_PORT

#if CONFIG_EXAMPLE_SPI_CAM_1_XCLK_USE_LEDC
#define EXAMPLE_SPI_CAM_1_XCLK_RESOURCE                 ESP_CAM_SENSOR_XCLK_LEDC
#elif CONFIG_EXAMPLE_SPI_CAM_1_XCLK_USE_CLOCK_ROUTER
#define EXAMPLE_SPI_CAM_1_XCLK_RESOURCE                 ESP_CAM_SENSOR_XCLK_ESP_CLOCK_ROUTER
#endif /* CONFIG_EXAMPLE_SPI_CAM_1_XCLK_USE_LEDC */

#if defined(EXAMPLE_SPI_CAM_1_XCLK_PIN) && EXAMPLE_SPI_CAM_1_XCLK_PIN >= 0
#define EXAMPLE_SPI_CAM_1_XCLK_FREQ                     CONFIG_EXAMPLE_SPI_CAM_1_XCLK_FREQ
#else
#define EXAMPLE_SPI_CAM_1_XCLK_FREQ                     0
#endif /* EXAMPLE_SPI_CAM_1_XCLK_PIN >= 0 */

#if CONFIG_EXAMPLE_SPI_CAM_1_XCLK_USE_LEDC
#define EXAMPLE_SPI_CAM_1_XCLK_TIMER                    CONFIG_EXAMPLE_SPI_CAM_1_XCLK_TIMER
#define EXAMPLE_SPI_CAM_1_XCLK_TIMER_CHANNEL            CONFIG_EXAMPLE_SPI_CAM_1_XCLK_TIMER_CHANNEL
#endif /* CONFIG_EXAMPLE_SPI_CAM_1_XCLK_USE_LEDC */
#endif /* CONFIG_EXAMPLE_ENABLE_SPI_CAM_1_SENSOR */
#endif /* CONFIG_EXAMPLE_ENABLE_SPI_CAM_0_SENSOR */

/**
 * @brief USB-UVC摄像头传感器通用配置
 */
#if CONFIG_EXAMPLE_ENABLE_USB_UVC_CAM_SENSOR
#define EXAMPLE_ENABLE_USB_UVC_CAM_SENSOR               1
#endif

/**
 * @brief 摄像头设备路径配置(根据启用的传感器类型自动选择)
 */
#if EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
#define EXAMPLE_CAM_DEV_PATH                            ESP_VIDEO_MIPI_CSI_DEVICE_NAME
#elif EXAMPLE_ENABLE_DVP_CAM_SENSOR
#define EXAMPLE_CAM_DEV_PATH                            ESP_VIDEO_DVP_DEVICE_NAME
#elif EXAMPLE_ENABLE_SPI_CAM_SENSOR
#define EXAMPLE_CAM_DEV_PATH                            ESP_VIDEO_SPI_DEVICE_NAME
#elif EXAMPLE_ENABLE_USB_UVC_CAM_SENSOR
#define EXAMPLE_CAM_DEV_PATH                            ESP_VIDEO_USB_UVC_DEVICE_NAME(0)
#else
#define EXAMPLE_CAM_DEV_PATH                            ESP_VIDEO_SPI_DEVICE_NAME
#endif /* CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR */

/**
 * @brief TinyUSB MSC(大容量存储类)存储配置
 *
 * @note 仅在启用了USB MSC存储且TinyUSB MSC使能时才会定义此宏
 */
#if !CONFIG_EXAMPLE_DISABLE_USB_MSC_STORAGE && CONFIG_TINYUSB_MSC_ENABLED
#define EXAMPLE_TINYUSB_MSC_STORAGE                     1
#endif /* !CONFIG_EXAMPLE_DISABLE_USB_MSC_STORAGE && CONFIG_TINYUSB_MSC_ENABLED */

/**
 * @brief 编码器句柄类型(不透明指针)
 */
typedef void *example_encoder_handle_t;

/**
 * @brief 存储句柄类型(不透明指针)
 */
typedef void *example_storage_handle_t;

/**
 * @brief 编码器配置结构体
 * @note 数据流向: 应用程序填充配置 -> example_encoder_init() -> 初始化编码器
 */
typedef struct example_encoder_config {
    uint32_t width;             /**< 输入图像宽度(像素) */
    uint32_t height;            /**< 输入图像高度(像素) */
    uint32_t pixel_format;      /**< 输入图像的V4L2像素格式(如V4L2_PIX_FMT_RGB565、V4L2_PIX_FMT_UYVY等) */
    uint8_t quality;            /**< JPEG图像质量(0-100), 数值越大质量越好 */
} example_encoder_config_t;

/**
 * @brief 初始化视频系统
 * @note 数据流向: 配置摄像头接口(MIPI-CSI/DVP/SPI) -> 分配XCLK时钟 -> 初始化I2C总线 -> esp_video_init()注册设备
 * @note 根据menuconfig配置, 可选择初始化MIPI-CSI、DVP或SPI接口的摄像头传感器
 *
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t example_video_init(void);

/**
 * @brief 反初始化视频系统, 释放视频设备资源
 *
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t example_video_deinit(void);

/**
 * @brief 初始化JPEG编码器
 *
 * @param config 编码器配置(宽高、像素格式、质量等)
 * @param ret_handle 返回的编码器句柄
 *
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t example_encoder_init(example_encoder_config_t *config, example_encoder_handle_t *ret_handle);

/**
 * @brief 获取编码器输出缓冲区(分配内存)
 *
 * @param handle 编码器句柄
 * @param buf 输出缓冲区指针(二级指针, 函数内部分配内存)
 * @param size 输出缓冲区大小(由函数填充)
 *
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t example_encoder_alloc_output_buffer(example_encoder_handle_t handle, uint8_t **buf, uint32_t *size);

/**
 * @brief 释放编码器输出缓冲区
 *
 * @param handle 编码器句柄
 * @param buf 要释放的输出缓冲区指针
 *
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t example_encoder_free_output_buffer(example_encoder_handle_t handle, uint8_t *buf);

/**
 * @brief 执行JPEG编码处理
 * @note 数据流向: src_buf(原始帧) -> 编码器 -> dst_buf(JPEG数据), dst_size_out输出实际编码大小
 *
 * @param handle 编码器句柄
 * @param src_buf 源缓冲区(原始图像数据)
 * @param src_size 源缓冲区大小(字节)
 * @param dst_buf 目标缓冲区(编码后的JPEG数据)
 * @param dst_size 目标缓冲区大小(字节)
 * @param dst_size_out 输出: 编码后JPEG数据的实际大小(字节)
 *
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t example_encoder_process(example_encoder_handle_t handle, uint8_t *src_buf, uint32_t src_size, uint8_t *dst_buf, uint32_t dst_size, uint32_t *dst_size_out);

/**
 * @brief 设置JPEG编码质量
 *
 * @param handle 编码器句柄
 * @param quality JPEG质量(0-100), 数值越大质量越好
 *
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t example_encoder_set_jpeg_quality(example_encoder_handle_t handle, uint8_t quality);

/**
 * @brief 反初始化编码器, 释放所有资源
 *
 * @param handle 编码器句柄
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t example_encoder_deinit(example_encoder_handle_t handle);

/**
 * @brief 初始化SPI Flash并挂载FATFS文件系统
 * @note 数据流向: 分配fatfs结构体 -> 初始化SPI Flash(磨损均衡) -> 挂载FATFS -> 返回句柄
 *
 * @param handle_ret 返回的存储句柄指针
 *
 * @return ESP_OK 成功, 否则返回错误代码
 *
 * @note: 此函数不是线程安全的
 */
esp_err_t example_mount_fatfs_to_spiflash(example_storage_handle_t *handle_ret);

/**
 * @brief 从FATFS卸载SPI Flash并释放资源
 *
 * @param handle 存储句柄
 *
 * @return ESP_OK 成功, 否则返回错误代码
 *
 * @note: 此函数不是线程安全的
 */
esp_err_t example_unmount_fatfs_in_spiflash(example_storage_handle_t handle);

#if EXAMPLE_TINYUSB_MSC_STORAGE
/**
 * @brief 初始化MSC大容量存储并挂载到SPI Flash
 * @note 数据流向: USB主机 <-> TinyUSB MSC <-> SPI Flash存储介质
 *
 * @param handle_ret 返回的存储句柄指针
 *
 * @return ESP_OK 成功, 否则返回错误代码
 *
 * @note: 此函数不是线程安全的
 */
esp_err_t example_mount_msc_to_spiflash(example_storage_handle_t *handle_ret);

/**
 * @brief 从SPI Flash卸载MSC大容量存储
 *
 * @param handle 存储句柄
 *
 * @return ESP_OK 成功, 否则返回错误代码
 *
 * @note: 此函数不是线程安全的
 */
esp_err_t example_unmount_msc_from_spiflash(example_storage_handle_t handle);
#endif /* EXAMPLE_TINYUSB_MSC_STORAGE */

#if CONFIG_SOC_SDMMC_HOST_SUPPORTED
/**
 * @brief 初始化SD卡并挂载FATFS文件系统
 * @note 数据流向: 分配fatfs结构体 -> 初始化SDMMC主机 -> 挂载FATFS -> 返回句柄
 *
 * @param handle_ret 返回的存储句柄指针
 *
 * @return ESP_OK 成功, 否则返回错误代码
 *
 * @note: 此函数不是线程安全的
 */
esp_err_t example_mount_fatfs_to_mmc(example_storage_handle_t *handle_ret);

/**
 * @brief 从SD卡卸载FATFS文件系统并反初始化SD卡
 *
 * @param handle 存储句柄
 *
 * @return ESP_OK 成功, 否则返回错误代码
 *
 * @note: 此函数不是线程安全的
 */
esp_err_t example_unmount_fatfs_in_mmc(example_storage_handle_t handle);

#if EXAMPLE_TINYUSB_MSC_STORAGE
/**
 * @brief 初始化MSC大容量存储并挂载到SD卡
 *
 * @param handle_ret 返回的存储句柄指针
 *
 * @return ESP_OK 成功, 否则返回错误代码
 *
 * @note: 此函数不是线程安全的
 */
esp_err_t example_mount_msc_to_mmc(example_storage_handle_t *handle_ret);

/**
 * @brief 从SD卡卸载MSC大容量存储
 *
 * @param handle 存储句柄
 *
 * @return ESP_OK 成功, 否则返回错误代码
 *
 * @note: 此函数不是线程安全的
 */
esp_err_t example_unmount_msc_from_mmc(example_storage_handle_t handle);
#endif /* EXAMPLE_TINYUSB_MSC_STORAGE */
#endif /* CONFIG_SOC_SDMMC_HOST_SUPPORTED */

#if EXAMPLE_TINYUSB_MSC_STORAGE
/**
 * @brief 检查MSC存储是否正在被USB主机使用
 *
 * @param handle 存储句柄
 * @param is_in_use 输出: 存储是否被USB主机使用中
 *
 * @return ESP_OK 成功, 否则返回错误代码
 *
 * @note: 此函数不是线程安全的
 */
esp_err_t example_msc_storage_in_use_by_usb_host(example_storage_handle_t handle, bool *is_in_use);
#endif /* EXAMPLE_TINYUSB_MSC_STORAGE */

/**
 * @brief 获取存储容量
 *
 * @param handle 存储句柄
 * @param capacity 输出: 存储容量(字节)
 *
 * @return ESP_OK 成功, 否则返回错误代码
 *
 * @note: 此函数不是线程安全的
 */
esp_err_t example_storage_get_capacity(example_storage_handle_t handle, uint64_t *capacity);

#ifdef __cplusplus
}
#endif
