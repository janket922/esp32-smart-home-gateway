#ifndef _SVC_ISOTP_H
#define _SVC_ISOTP_H

#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_isotp.h"
#define TWAI_SENDER_TX_GPIO GPIO_NUM_3
#define TWAI_SENDER_RX_GPIO GPIO_NUM_10
#define TWAI_QUEUE_DEPTH 8
#define TWAI_BITRATE 1000000
#define TWAI_SAMPLING_POINT 833
#define TWAI_QUEUE_LEN 10

// Message IDs
#define TWAI_DATA_ID 0x098
#define TWAI_DATA_LEN 8

//isotp
#define ISOTP_RECV_BUF_SIZE 512
#define ISOTP_SEND_BUF_SIZE 512

typedef struct
{
	twai_frame_t frame;
	uint8_t data[TWAI_FRAME_MAX_LEN];
} twai_sender_data_t;

#define SENSOR_DATA_CANID 0x717
#ifdef SENSOR_DATA_CANID
#define SENSOR_DATA_LEN 12
#endif
typedef struct
{
	float temperature;
	float humidity;
} AHT20_Data_t;

typedef struct
{
	AHT20_Data_t aht20_data;
	uint16_t light;
} Sensor_data_t;

typedef struct
{
	uint32_t id;
	uint8_t data[TWAI_FRAME_MAX_LEN];
	uint8_t dlc;
} CAN_packet_t;

esp_err_t Svc_Isotp_Send(uint8_t *data, size_t len);

void Svc_Isotp_Init(void);
esp_err_t Svc_Isotp_Poll(void);
esp_err_t Svc_Isotp_Receive(uint8_t *pdata,
							uint32_t buffer_size,
							uint32_t *received_size);
uint32_t Svc_Isotp_Processing(uint8_t *pdata, uint32_t len);

uint8_t Svc_Isotp_TwaiError_Detect(void);
void Svc_Isotp_Log_Diagnostics(void);


#endif
