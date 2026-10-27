#ifndef _SVC_ENCODE_BASE64_H_
#define _SVC_ENCODE_BASE64_H_

#include <stdint.h>
#include <string.h>
#include "Svc_Video.h"
#include "mbedtls/base64.h"

#define VIDEO_MAX_FRAME_SIZE 30 * 1024
#define VIDEO_JSON_PREFIX "{\"params\": {\"image\": \""
#define VIDEO_JSON_SUFFIX "\"}}"
#define MAX_ENCODER_LEN ((size_t)(sizeof(VIDEO_JSON_PREFIX) - 1 + \
    4 * ((VIDEO_MAX_FRAME_SIZE + 2) / 3) + sizeof(VIDEO_JSON_SUFFIX)))

/* Caller supplies MAX_ENCODER_LEN bytes. Failure produces an empty string. */
esp_err_t svc_encode_base64_with_jsonfilled(const svc_frame_packet_t *p, unsigned char encoded_buf[]);
esp_err_t svc_encode_base64(const svc_frame_packet_t *p, unsigned char encoded_buf[]);
#endif
