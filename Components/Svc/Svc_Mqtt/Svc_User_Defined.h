#ifndef _SVC_USER_DEFINED_H_
#define _SVC_USER_DEFINED_H_

/* Real values belong in the ignored Svc_User_Defined.local.h. */
#if !defined(SVC_USE_PLACEHOLDERS) && __has_include("Svc_User_Defined.local.h")
#include "Svc_User_Defined.local.h"
#else
#define USE_WIFI_SSID "XX"
#define USE_WIFI_PASSWORD "XX"
#define USE_WIFI_CONNECT_MAX_RETRY 5
#define USE_WIFI_AUTH_MODE WIFI_AUTH_WPA2_PSK
#define CONFIG_BROKER_ONENET_IOT 0
#define CONFIG_BROKER_BAIDU_IOT 1
#define USE_MQTT_BROKER_URI "mqtt://XX:1883"
#define USE_CLIENTID "XX"
#define USE_USERNAME "XX"
#define USE_MQTT_PASSWORD "XX"
#define USE_VIDEO_TOPIC_PUBLISH "$iot/XX/user/video"
#define USE_SENSOR_TOPIC_PUBLISH "$iot/XX/user/Sensor"
#define USE_TOPIC_SUBSCERIBE "$iot/XX/events"
#endif
#endif
