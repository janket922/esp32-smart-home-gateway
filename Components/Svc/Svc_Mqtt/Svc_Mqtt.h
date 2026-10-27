#ifndef _SVC_MQTT_H_
#define _SVC_MQTT_H_

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "WIFI_CONNECT.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "Svc_User_Defined.h"

#include "esp_crt_bundle.h"
#include "mqtt_client.h"
#include "cJSON.h"

void Svc_Mqtt_Start(void);
int Svc_Mqtt_Publish(const char *topic, const char *data, int data_len, int qos, int retain);
int Svc_Mqtt_Publish_For_VideoStream(const char *topic, const char *data);
#endif 
