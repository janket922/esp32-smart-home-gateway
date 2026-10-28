#include "Svc_Isotp.h"

static const char *TAG = "twai_sender";
esp_isotp_handle_t isotp_handle = NULL;
twai_node_handle_t node = NULL;
static bool recovery_requested;
esp_err_t Svc_Isotp_Send(uint8_t *data, size_t len)
{
	if (data == NULL)
	{
		return ESP_ERR_INVALID_ARG;
	}
	return esp_isotp_send(isotp_handle, data, len);
}

void Svc_Isotp_Init(void)
{
	if (isotp_handle != NULL) return;

	printf("===================TWAI Sender Example Starting...===================\n");
	// Configure TWAI node
	twai_onchip_node_config_t node_config = {
		.io_cfg = {
			.tx = TWAI_SENDER_TX_GPIO,
			.rx = TWAI_SENDER_RX_GPIO,
			.quanta_clk_out = GPIO_NUM_NC,
			.bus_off_indicator = GPIO_NUM_NC,
		},
		.bit_timing = {
			.bitrate = TWAI_BITRATE,
			.sp_permill = TWAI_SAMPLING_POINT,
		},
		.intr_priority = 0,
		.fail_retry_cnt = 3,
		.tx_queue_depth = TWAI_QUEUE_DEPTH,
		.flags = {
			.enable_loopback = 0,
			.enable_self_test = 0,
			.enable_listen_only = 0,
			.no_receive_rtr = 0,
			.sleep_allow_pd = 0,
		},
	};

	// Create TWAI node
	ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node));

	// 配置过滤
	twai_mask_filter_config_t filter_config = {0};
	filter_config.dual_filter = false;
	filter_config.no_fd = true;
	filter_config.is_ext = false;
	filter_config.id = 0x000;
	filter_config.mask = 0x000;

	ESP_ERROR_CHECK(twai_node_config_mask_filter(node, 0, &filter_config));

	// isotp
	esp_isotp_config_t isotp_config = {
		.rx_buffer_size = ISOTP_RECV_BUF_SIZE,
		.tx_buffer_size = ISOTP_SEND_BUF_SIZE,
		.rx_id = SENSOR_DATA_CANID,
		.tx_id = TWAI_DATA_ID};

	ESP_ERROR_CHECK(esp_isotp_new_transport(node, &isotp_config, &isotp_handle));
	// esp_isotp_new_transport内部已经调用twai_node_enable，无需重复调用
	// ESP_ERROR_CHECK(twai_node_enable(node));
	ESP_LOGI(TAG, "TWAI Sender started");
}

esp_err_t Svc_Isotp_Poll(void)
{
	if (isotp_handle == NULL)
	{
		return ESP_ERR_INVALID_STATE;
	}

	if (Svc_Isotp_TwaiError_Detect()) return ESP_ERR_NOT_FINISHED;
	return esp_isotp_poll(isotp_handle);
}

esp_err_t Svc_Isotp_Receive(uint8_t *pdata,
							uint32_t buffer_size,
							uint32_t *received_size)
{
	if (pdata == NULL ||
		received_size == NULL ||
		buffer_size == 0)
	{
		return ESP_ERR_INVALID_ARG;
	}

	*received_size = 0;

	if (isotp_handle == NULL)
	{
		return ESP_ERR_INVALID_STATE;
	}

	return esp_isotp_receive(isotp_handle,
							 pdata,
							 buffer_size,
							 received_size);
}

uint8_t Svc_Isotp_TwaiError_Detect(void)
{
    if (node == NULL) return 1;
    twai_node_status_t status = {0};
    if (twai_node_get_info(node, &status, NULL) != ESP_OK) return 1;
    if (status.state != TWAI_ERROR_BUS_OFF)
    {
        if (recovery_requested) ESP_LOGI(TAG, "CAN bus recovery completed");
        recovery_requested = false;
        return 0;
    }
    if (!recovery_requested)
    {
        esp_err_t ret = twai_node_recover(node);
        if (ret == ESP_OK)
        {
            recovery_requested = true;
            ESP_LOGW(TAG, "CAN bus-off: recovery requested; node and ISO-TP handle retained");
        }
    }
    return 1;
}

void Svc_Isotp_Log_Diagnostics(void)
{
    if (node == NULL || isotp_handle == NULL) return;
    twai_node_status_t status = {0};
    twai_node_record_t record = {0};
    esp_isotp_diagnostics_t diag = {0};
    if (twai_node_get_info(node, &status, &record) != ESP_OK ||
        esp_isotp_get_diagnostics(isotp_handle, &diag) != ESP_OK) return;
    ESP_LOGI("ISOTP_DIAG",
             "state=%d TEC=%u REC=%u buserr=%lu canrx=%lu matched=%lu rxdrop=%lu "
             "tx_submit_fail=%lu tx_fail=%lu messages=%lu pending=%lu gap_max_us=%lu rx_state=%d rx_result=%d",
             (int)status.state, (unsigned)status.tx_error_count, (unsigned)status.rx_error_count,
             (unsigned long)record.bus_err_num, (unsigned long)diag.can_rx_frames,
             (unsigned long)diag.matched_rx_frames, (unsigned long)diag.rx_queue_drops,
             (unsigned long)diag.tx_submit_failures, (unsigned long)diag.tx_failed_frames,
             (unsigned long)diag.complete_messages, (unsigned long)diag.pending_rx_frames,
             (unsigned long)diag.max_poll_gap_us, diag.receive_status, diag.receive_result);
}
