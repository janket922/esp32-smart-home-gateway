/*
 * SPDX-FileCopyrightText: 2025 乐鑫科技（上海）有限公司
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_video_ioctl.h"
#include "esp_video_init.h"
#include "esp_cam_sensor_xclk.h"
#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
#include "driver/jpeg_encode.h"
#else
#include "esp_jpeg_enc.h"
#endif
#include "example_video_common.h"

/**
 * @brief JPEG编码器内部结构体
 * @note 数据流向: 原始图像帧(RAW/RGB/YUV) -> jpeg_enc_config/jpeg_handle -> JPEG压缩数据 -> jpeg_out_buf
 * @note 支持硬件JPEG编码器(ESP32-P4)或软件JPEG编码器两种模式, 通过menuconfig选择
 */
typedef struct example_encoder {
#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    jpeg_encode_cfg_t jpeg_enc_config;  /* 硬件JPEG编码器配置(图像质量、源格式、采样方式等) */
#else
    jpeg_enc_handle_t jpeg_handle;      /* 软件JPEG编码器句柄 */
#endif
    uint32_t jpeg_out_buf_size;         /* JPEG输出缓冲区的建议大小(字节) */
} example_encoder_t;

static const char *TAG = "example_encoder";

#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
/**
 * @brief 所有视频流共享的JPEG硬件编码器句柄(单例模式)
 */
static jpeg_encoder_handle_t s_jpeg_hw_handle;
static uint32_t s_jpeg_hw_ref_count;    /* 硬件编码器引用计数, 用于管理多个视频流共享同一个硬件编码器实例 */
#endif

/**
 * @brief 初始化JPEG编码器
 *
 * @param config 编码器配置参数(图像宽高、像素格式、质量等)
 * @param ret_handle 返回的编码器句柄
 *
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t example_encoder_init(example_encoder_config_t *config, example_encoder_handle_t *ret_handle)
{
    if (!config || !ret_handle) {
        ESP_LOGE(TAG, "invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;
    example_encoder_t *encoder;
    uint32_t jpeg_enc_input_src_size;
#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    jpeg_encode_cfg_t jpeg_enc_config = {0};
    jpeg_encoder_handle_t jpeg_handle = NULL;
#else
    jpeg_enc_handle_t jpeg_handle = NULL;
    jpeg_enc_config_t jpeg_enc_config = {0};
#endif

    jpeg_enc_config.height = config->height;
    jpeg_enc_config.width = config->width;

#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    jpeg_enc_config.image_quality = config->quality;

    switch (config->pixel_format) {
    case V4L2_PIX_FMT_SBGGR8: // 将raw8当作灰度图处理, 仅用于测试
    case V4L2_PIX_FMT_GREY:
        jpeg_enc_config.src_type = JPEG_ENCODE_IN_FORMAT_GRAY;
        jpeg_enc_config.sub_sample = JPEG_DOWN_SAMPLING_GRAY;
        jpeg_enc_input_src_size = config->width * config->height;
        break;
    case V4L2_PIX_FMT_RGB565:
        jpeg_enc_config.src_type = JPEG_ENCODE_IN_FORMAT_RGB565;
        jpeg_enc_config.sub_sample = JPEG_DOWN_SAMPLING_YUV422;
        jpeg_enc_input_src_size = config->width * config->height * 2;
        break;
    case V4L2_PIX_FMT_RGB24:
        jpeg_enc_config.src_type = JPEG_ENCODE_IN_FORMAT_RGB888;
        jpeg_enc_config.sub_sample = JPEG_DOWN_SAMPLING_YUV444;
        jpeg_enc_input_src_size = config->width * config->height * 3;
        break;
    case V4L2_PIX_FMT_UYVY:
        jpeg_enc_config.src_type = JPEG_ENCODE_IN_FORMAT_YUV422;
        jpeg_enc_config.sub_sample = JPEG_DOWN_SAMPLING_YUV422;
        jpeg_enc_input_src_size = config->width * config->height * 2;
        break;
#if CONFIG_ESP32P4_REV_MIN_FULL >= 300
    case V4L2_PIX_FMT_YUV420:
        jpeg_enc_config.src_type = JPEG_ENCODE_IN_FORMAT_YUV420;
        jpeg_enc_config.sub_sample = JPEG_DOWN_SAMPLING_YUV420;
        jpeg_enc_input_src_size = config->width * config->height * 3 / 2;
        break;
    case V4L2_PIX_FMT_YUV444:
        jpeg_enc_config.src_type = JPEG_ENCODE_IN_FORMAT_YUV444;
        jpeg_enc_config.sub_sample = JPEG_DOWN_SAMPLING_YUV444;
        jpeg_enc_input_src_size = config->width * config->height * 3;
        break;
#endif
    default:
        ESP_LOGE(TAG, "Unsupported format");
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (!s_jpeg_hw_handle) {
        jpeg_encode_engine_cfg_t encode_eng_cfg = {
            .timeout_ms = 5000,
        };
        ESP_RETURN_ON_ERROR(jpeg_new_encoder_engine(&encode_eng_cfg, &jpeg_handle), TAG, "failed to create jpeg encoder engine");
    }
#else
    jpeg_enc_config.quality = config->quality;

    switch (config->pixel_format) {
    case V4L2_PIX_FMT_SBGGR8:
    case V4L2_PIX_FMT_GREY:
        jpeg_enc_config.src_type = JPEG_PIXEL_FORMAT_GRAY;
        jpeg_enc_config.subsampling = JPEG_SUBSAMPLE_GRAY;
        jpeg_enc_input_src_size = config->width * config->height;
        break;
    case V4L2_PIX_FMT_UYVY:
        jpeg_enc_config.src_type = JPEG_PIXEL_FORMAT_CbYCrY;
        jpeg_enc_config.subsampling = JPEG_SUBSAMPLE_422;
        jpeg_enc_input_src_size = config->width * config->height * 2;
        break;
    case V4L2_PIX_FMT_YUYV:
        jpeg_enc_config.src_type = JPEG_PIXEL_FORMAT_YCbYCr;
        jpeg_enc_config.subsampling = JPEG_SUBSAMPLE_422;
        jpeg_enc_input_src_size = config->width * config->height * 2;
        break;
    case V4L2_PIX_FMT_RGB565:
        jpeg_enc_config.src_type = JPEG_PIXEL_FORMAT_RGB565_LE;
        jpeg_enc_config.subsampling = JPEG_SUBSAMPLE_422;
        jpeg_enc_input_src_size = config->width * config->height * 2;
        break;
    case V4L2_PIX_FMT_RGB565X:
        jpeg_enc_config.src_type = JPEG_PIXEL_FORMAT_RGB565_BE;
        jpeg_enc_config.subsampling = JPEG_SUBSAMPLE_422;
        jpeg_enc_input_src_size = config->width * config->height * 2;
        break;
    default:
        ESP_LOGE(TAG, "Unsupported format");
        return ESP_ERR_NOT_SUPPORTED;
    }

    ESP_RETURN_ON_ERROR(jpeg_enc_open(&jpeg_enc_config, &jpeg_handle), TAG, "failed to open jpeg encoder");
#endif

    encoder = (example_encoder_t *)calloc(1, sizeof(example_encoder_t));
    ESP_GOTO_ON_FALSE(encoder, ESP_ERR_NO_MEM, fail0, TAG, "failed to alloc example encoder");

#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    encoder->jpeg_enc_config = jpeg_enc_config;
    if (!s_jpeg_hw_ref_count) {
        s_jpeg_hw_ref_count++;
        s_jpeg_hw_handle = jpeg_handle;
    }
#else
    encoder->jpeg_handle = jpeg_handle;
#endif

    encoder->jpeg_out_buf_size = jpeg_enc_input_src_size * 3 / 4;

    *ret_handle = encoder;

    return ESP_OK;

fail0:
#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    jpeg_del_encoder_engine(jpeg_handle);
#else
    jpeg_enc_close(jpeg_handle);
#endif
    return ret;
}

/**
 * @brief 获取编码器输出缓冲区
 * @note 数据流向: 根据编码器配置分配内存 -> buf指向分配的输出缓冲区 -> 供编码器写入JPEG数据
 * @note 硬件编码器使用专用内存分配器(jpeg_alloc_encoder_mem), 软件编码器使用对齐分配(jpeg_calloc_align)
 *
 * @param handle 编码器句柄
 * @param buf 输出缓冲区指针(二级指针, 函数内部分配)
 * @param size 输出缓冲区大小(由函数填充实际分配大小)
 *
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t example_encoder_alloc_output_buffer(example_encoder_handle_t handle, uint8_t **buf, uint32_t *size)
{
#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    if (!s_jpeg_hw_ref_count) {
        ESP_LOGE(TAG, "jpeg hardware encoder is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
#endif

    uint8_t *jpeg_out_buf;
    example_encoder_t *encoder = (example_encoder_t *)handle;
    if (!encoder || !buf || !size) {
        ESP_LOGE(TAG, "invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    size_t jpeg_out_buf_size;
    jpeg_encode_memory_alloc_cfg_t jpeg_enc_output_mem_cfg = {
        .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,
    };

    // 注意: JPEG_ENC_QUALITY越大表示图像质量越高, 因此需要增加分配的缓冲区大小
    jpeg_out_buf = (uint8_t *)jpeg_alloc_encoder_mem(encoder->jpeg_out_buf_size, &jpeg_enc_output_mem_cfg, &jpeg_out_buf_size);
    ESP_RETURN_ON_FALSE(jpeg_out_buf, ESP_ERR_NO_MEM, TAG, "failed to alloc jpeg output buf");
#else
    uint32_t jpeg_out_buf_size;

    jpeg_out_buf = jpeg_calloc_align(encoder->jpeg_out_buf_size, 128);
    ESP_RETURN_ON_FALSE(jpeg_out_buf, ESP_ERR_NO_MEM, TAG, "failed to alloc jpeg output buf");
    jpeg_out_buf_size = encoder->jpeg_out_buf_size;
#endif

    *buf = jpeg_out_buf;
    *size = jpeg_out_buf_size;

    return ESP_OK;
}

/**
 * @brief 释放编码器输出缓冲区
 *
 * @param handle 编码器句柄
 * @param buf 要释放的输出缓冲区指针
 *
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t example_encoder_free_output_buffer(example_encoder_handle_t handle, uint8_t *buf)
{
#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    if (!s_jpeg_hw_ref_count) {
        ESP_LOGE(TAG, "jpeg hardware encoder is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
#endif

    if (!buf || !handle) {
        ESP_LOGE(TAG, "invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    free(buf);
#else
    jpeg_free_align(buf);
#endif
    return ESP_OK;
}

/**
 * @brief 执行JPEG编码处理
 * @note 数据流向: src_buf(原始帧) -> JPEG编码器 -> dst_buf(JPEG压缩数据), dst_size_out输出实际编码后大小
 *
 * @param handle 编码器句柄
 * @param src_buf 源缓冲区(原始图像数据, 如RGB/YUV)
 * @param src_size 源缓冲区大小(字节)
 * @param dst_buf 目标缓冲区(编码后的JPEG数据)
 * @param dst_size 目标缓冲区大小(字节)
 * @param dst_size_out 输出: 编码后JPEG数据的实际大小(字节)
 *
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t example_encoder_process(example_encoder_handle_t handle, uint8_t *src_buf, uint32_t src_size,
                                  uint8_t *dst_buf, uint32_t dst_size, uint32_t *dst_size_out)
{
#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    if (!s_jpeg_hw_ref_count) {
        ESP_LOGE(TAG, "jpeg hardware encoder is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
#endif

    if (!handle || !src_buf || !src_size || !dst_buf || !dst_size || !dst_size_out) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;
    example_encoder_t *encoder = (example_encoder_t *)handle;

#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    ret = jpeg_encoder_process(s_jpeg_hw_handle, &encoder->jpeg_enc_config, src_buf, src_size, dst_buf, dst_size, dst_size_out);
#else
    ret = jpeg_enc_process(encoder->jpeg_handle, src_buf, src_size, dst_buf, dst_size, (int *)dst_size_out);
#endif

    return ret;
}

/**
 * @brief 设置JPEG编码质量
 *
 * @param handle 编码器句柄
 * @param quality JPEG质量(0-100), 数值越大质量越高, 文件也越大
 *
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t example_encoder_set_jpeg_quality(example_encoder_handle_t handle, uint8_t quality)
{
    esp_err_t ret = ESP_OK;
    example_encoder_t *encoder = (example_encoder_t *)handle;
    if (!encoder) {
        ESP_LOGE(TAG, "example encoder is not initialized");
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    encoder->jpeg_enc_config.image_quality = quality;
#else
    ret = jpeg_enc_set_quality(encoder->jpeg_handle, quality);
#endif
    return ret;
}

/**
 * @brief 反初始化编码器, 释放所有资源
 * @note 硬件编码器使用引用计数管理, 当所有视频流都释放后才会真正销毁硬件编码器引擎
 *
 * @param handle 编码器句柄
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t example_encoder_deinit(example_encoder_handle_t handle)
{
    example_encoder_t *encoder = (example_encoder_t *)handle;
    if (!encoder) {
        ESP_LOGE(TAG, "example encoder is not initialized");
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    if (s_jpeg_hw_ref_count) {
        s_jpeg_hw_ref_count--;
        if (!s_jpeg_hw_ref_count) {
            jpeg_del_encoder_engine(s_jpeg_hw_handle);
            s_jpeg_hw_handle = NULL;
        }
    } else {
        ESP_LOGW(TAG, "jpeg hardware encoder ref count already 0, possible double deinit");
    }
#else
    jpeg_enc_close(encoder->jpeg_handle);
#endif
    free(encoder);

    return ESP_OK;
}
