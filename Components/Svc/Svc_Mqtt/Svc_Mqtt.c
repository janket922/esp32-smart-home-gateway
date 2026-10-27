#include "Svc_Mqtt.h"

static const char *TAG = "mqtts_example";
char json_payload[128];
static esp_mqtt_client_handle_t _client_;


/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
	ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
	esp_mqtt_event_handle_t event = event_data;
	if (event == NULL) return;
	esp_mqtt_client_handle_t client = event->client;
	switch ((esp_mqtt_event_id_t)event_id)
	{
	case MQTT_EVENT_CONNECTED:
		esp_mqtt_client_subscribe(client, USE_TOPIC_SUBSCERIBE, 0);
		break;
	case MQTT_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
		break;

	case MQTT_EVENT_SUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_UNSUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_PUBLISHED:
		ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_DATA:
		ESP_LOGI(TAG, "MQTT_EVENT_DATA");

		break;
	case MQTT_EVENT_ERROR:
		ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
		if (event->error_handle == NULL) break;
		if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
		{
			ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
			ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
			ESP_LOGI(TAG, "Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
					 strerror(event->error_handle->esp_transport_sock_errno));
		}
		else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
		{
			ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
		}
		else
		{
			ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
		}
		break;
	default:
		ESP_LOGI(TAG, "Other event id:%d", event->event_id);
		break;
	}
}

void Svc_Mqtt_Start(void)
{
	if (_client_ != NULL) return;
	//connect wifi
	wifi_sta_cfg_t cfg = {
		.ssid = USE_WIFI_SSID,
		.password = USE_WIFI_PASSWORD,
		.max_retry = USE_WIFI_CONNECT_MAX_RETRY,
		.auth_mode = USE_WIFI_AUTH_MODE,
	};
	wifi_sta_init(&cfg);
	ESP_LOGI(TAG, "Wifi Connected");

	//connect mqtt
	const esp_mqtt_client_config_t mqtt_cfg = {
		.broker = {
			.address.uri = USE_MQTT_BROKER_URI,
			.verification.crt_bundle_attach = esp_crt_bundle_attach, /* Use built-in certificate bundle */
		},
		.credentials = {
			.username = USE_USERNAME,
			.client_id = USE_CLIENTID,
			.authentication.password = USE_MQTT_PASSWORD,
		},
	};

	ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
	_client_ = esp_mqtt_client_init(&mqtt_cfg);
	if (_client_ == NULL)
	{
		ESP_LOGE(TAG, "MQTT client allocation failed");
		return;
	}
	/* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
	ESP_ERROR_CHECK(esp_mqtt_client_register_event(_client_, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
	ESP_ERROR_CHECK(esp_mqtt_client_start(_client_));
}

int Svc_Mqtt_Publish(const char *topic, const char *data, int data_len, int qos, int retain)
{
	if (_client_ == NULL || topic == NULL || data == NULL || data_len < 0) return -1;
	return esp_mqtt_client_publish(_client_, topic, data, data_len, qos, retain);
}

int Svc_Mqtt_Publish_For_VideoStream(const char *topic, const char *data)
{
	return Svc_Mqtt_Publish(topic, data, 0, 0, 0);
}
