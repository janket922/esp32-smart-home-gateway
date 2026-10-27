#include "Video.h"

static const esp_video_init_dvp_config_t s_dvp_config = {
	.sccb_config = {
		.init_sccb = true,
		.freq = BSP_DVP_SCCB_I2C_FREQ,
		.i2c_config = {
			.port = BSP_DVP_SCCB_I2C_PORT,
			.scl_pin = BSP_DVP_SCCB_I2C_SCL_PIN,
			.sda_pin = BSP_DVP_SCCB_I2C_SDA_PIN,
		}},
	.reset_pin = BSP_DVP_CAM_SENSOR_RESET_PIN,
	.pwdn_pin = BSP_DVP_CAM_SENSOR_PWDN_PIN,
	.dvp_pin = {
		.data_width = CAM_CTLR_DATA_WIDTH_8,
		.data_io = {
			BSP_DVP_D0_PIN,
			BSP_DVP_D1_PIN,
			BSP_DVP_D2_PIN,
			BSP_DVP_D3_PIN,
			BSP_DVP_D4_PIN,
			BSP_DVP_D5_PIN,
			BSP_DVP_D6_PIN,
			BSP_DVP_D7_PIN,
		},
		.vsync_io = BSP_DVP_VSYNC_PIN,
		.de_io = BSP_DVP_DE_PIN,
		.pclk_io = BSP_DVP_PCLK_PIN,
		.xclk_io = BSP_DVP_XCLK_PIN,
	},
	.xclk_freq = BSP_DVP_XCLK_FREQ,
};

static const esp_video_init_config_t s_cam_config = {
#if BSP_ENABLE_DVP_CAM_SENSOR
	.dvp = &s_dvp_config,
#endif /* BSP_ENABLE_DVP_CAM_SENSOR */

};

static bool s_is_init = false;
static const char *TAG = "bsp_video";

/**
 * @brief 初始化摄像头
 *
 * @return 成功则返回ESP_OK，其他值表示失败
 * @note 会检查初始化状态标志, 避免重复初始化
 */
esp_err_t bsp_video_init(void)
{
	esp_err_t ret = ESP_FAIL;
	if (s_is_init == false)
	{
		XL9555_init();
		const esp_video_init_config_t *cam_config_ptr = &s_cam_config;
		ret = esp_video_init(cam_config_ptr);
		if (ret == ESP_OK)
		{
			s_is_init = true;
		}
	}
	return ret;
}

/**
 * @brief 反初始化视频系统, 释放视频设备资源
 * @note 数据流向: esp_video_deinit()停止视频设备 -> 停止XCLK时钟 -> 可选: 删除I2C总线
 * @note 会检查初始化状态标志, 避免重复反初始化
 *
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t bsp_video_deinit(void)
{
	esp_err_t ret = ESP_FAIL;
	if (s_is_init == true)
	{
		ret = esp_video_deinit();
		if (ret == ESP_OK)
		{
			s_is_init = false;
		}
	}
	return ret;
}

#define BSP_MMAP_BUFFER_COUNT 4
static int bsp_video_fd = -1;
static uint8_t *bsp_mmap_buf_addr[BSP_MMAP_BUFFER_COUNT];
static size_t bsp_mmap_buf_length[BSP_MMAP_BUFFER_COUNT];
static uint32_t bsp_mmap_buf_count;

esp_err_t bsp_video_off(void)
{
    if (bsp_video_fd < 0) return ESP_OK;
    for (uint32_t i = 0; i < BSP_MMAP_BUFFER_COUNT; ++i)
    {
        if (bsp_mmap_buf_addr[i] != NULL && bsp_mmap_buf_addr[i] != MAP_FAILED)
        {
            if (munmap(bsp_mmap_buf_addr[i], bsp_mmap_buf_length[i]) != 0)
                return ESP_FAIL;
            bsp_mmap_buf_addr[i] = NULL;
            bsp_mmap_buf_length[i] = 0;
        }
    }
    struct v4l2_requestbuffers req = {0};
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    int release_ret = ioctl(bsp_video_fd, VIDIOC_REQBUFS, &req);
    int close_ret = close(bsp_video_fd);
    bsp_video_fd = -1;
    bsp_mmap_buf_count = 0;
    return release_ret == 0 && close_ret == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t bsp_video_ready(void)
{
    if (bsp_video_fd >= 0) return ESP_ERR_INVALID_STATE;
    bsp_video_fd = open(ESP_VIDEO_DVP_DEVICE_NAME, O_RDWR);
    if (bsp_video_fd < 0) return ESP_FAIL;

    struct v4l2_format format = {0};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = 640;
    format.fmt.pix.height = 480;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
    format.fmt.pix.field = V4L2_FIELD_NONE;
    format.fmt.pix.sizeimage = 128 * 1024;
    format.fmt.pix.colorspace = V4L2_COLORSPACE_DEFAULT;
    if (ioctl(bsp_video_fd, VIDIOC_S_FMT, &format) != 0) goto fail;

    struct v4l2_requestbuffers req = {0};
    req.count = BSP_MMAP_BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(bsp_video_fd, VIDIOC_REQBUFS, &req) != 0) goto fail;
    if (req.count == 0 || req.count > BSP_MMAP_BUFFER_COUNT) goto fail;
    bsp_mmap_buf_count = req.count;
    for (uint32_t i = 0; i < bsp_mmap_buf_count; ++i)
    {
        struct v4l2_buffer buf = {0};
        buf.index = i;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(bsp_video_fd, VIDIOC_QUERYBUF, &buf) != 0 || buf.length == 0)
            goto fail;
        uint8_t *addr = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                             MAP_SHARED, bsp_video_fd, buf.m.offset);
        /* This ESP video implementation returns NULL on mapping failure. */
        if (addr == NULL || addr == MAP_FAILED) goto fail;
        bsp_mmap_buf_addr[i] = addr;
        bsp_mmap_buf_length[i] = buf.length;
        if (ioctl(bsp_video_fd, VIDIOC_QBUF, &buf) != 0) goto fail;
    }
    return ESP_OK;
fail:
    bsp_video_off();
    ESP_LOGE(TAG, "Camera buffer setup failed; resources released");
    return ESP_FAIL;
}

esp_err_t bsp_video_start(void)
{
    if (bsp_video_fd < 0) return ESP_ERR_INVALID_STATE;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    return ioctl(bsp_video_fd, VIDIOC_STREAMON, &type) == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t bsp_video_get_frame(bsp_frame_packet_t *p)
{
    if (p == NULL) return ESP_ERR_INVALID_ARG;
    p->length = 0;
    if (p->payload == NULL) return ESP_ERR_INVALID_ARG;
    if (bsp_video_fd < 0) return ESP_ERR_INVALID_STATE;

    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(bsp_video_fd, VIDIOC_DQBUF, &buf) != 0) return ESP_FAIL;
    esp_err_t ret = ESP_FAIL;
    /* Validate before indexing a mapping or copying into the frame pool. */
    if (buf.index < bsp_mmap_buf_count && buf.index < BSP_MMAP_BUFFER_COUNT &&
        bsp_mmap_buf_addr[buf.index] != NULL &&
        bsp_mmap_buf_addr[buf.index] != MAP_FAILED &&
        buf.bytesused > 0 && buf.bytesused <= BSP_VIDEO_MAX_FRAME_SIZE &&
        buf.bytesused <= bsp_mmap_buf_length[buf.index])
    {
        memcpy(p->payload, bsp_mmap_buf_addr[buf.index], buf.bytesused);
        p->length = buf.bytesused;
        ret = ESP_OK;
    }
    if (ioctl(bsp_video_fd, VIDIOC_QBUF, &buf) != 0)
    {
        p->length = 0;
        ret = ESP_FAIL;
    }
    return ret;
}

esp_err_t bsp_video_stop(void)
{
    if (bsp_video_fd < 0) return ESP_ERR_INVALID_STATE;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    return ioctl(bsp_video_fd, VIDIOC_STREAMOFF, &type) == 0 ? ESP_OK : ESP_FAIL;
}
