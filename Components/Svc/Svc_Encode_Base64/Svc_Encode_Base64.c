#include "Svc_Encode_Base64.h"

esp_err_t svc_encode_base64_with_jsonfilled(const svc_frame_packet_t *p, unsigned char encoded_buf[])
{
    if (encoded_buf == NULL) return ESP_ERR_INVALID_ARG;
    encoded_buf[0] = '\0';
    if (p == NULL || p->payload == NULL || p->length == 0 || p->length > VIDEO_MAX_FRAME_SIZE)
        return ESP_ERR_INVALID_ARG;

    const size_t header_len = sizeof(VIDEO_JSON_PREFIX) - 1;
    size_t need_len = 0;
    /* Mbed TLS also needs space for its own terminating NUL. */
    int ret = mbedtls_base64_encode(encoded_buf + header_len,
                                    MAX_ENCODER_LEN - header_len,
                                    &need_len, p->payload, p->length);
    if (ret != 0 || need_len > MAX_ENCODER_LEN - header_len - sizeof(VIDEO_JSON_SUFFIX))
        return ESP_FAIL;
    memcpy(encoded_buf, VIDEO_JSON_PREFIX, header_len);
    memcpy(encoded_buf + header_len + need_len, VIDEO_JSON_SUFFIX, sizeof(VIDEO_JSON_SUFFIX));
    return ESP_OK;
}

esp_err_t svc_encode_base64(const svc_frame_packet_t *p, unsigned char encoded_buf[])
{
    if (encoded_buf == NULL) return ESP_ERR_INVALID_ARG;
    encoded_buf[0] = '\0';
    if (p == NULL || p->payload == NULL || p->length > VIDEO_MAX_FRAME_SIZE)
        return ESP_ERR_INVALID_ARG;
    size_t need_len = 0;
    if (mbedtls_base64_encode(encoded_buf, MAX_ENCODER_LEN, &need_len, p->payload, p->length) != 0)
        return ESP_FAIL;
    encoded_buf[need_len] = '\0';
    return ESP_OK;
}
