/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 * Local integration fixes: persistent TWAI TX storage and task-owned ISO-TP state.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_twai.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_isotp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "isotp.h"

#define ISOTP_RX_QUEUE_DEPTH 32
#define ISOTP_TX_SLOT_COUNT 9
static const char *TAG = "esp_isotp";

typedef struct {
    twai_frame_t frame;
    uint8_t data[TWAI_FRAME_MAX_LEN];
} isotp_tx_slot_t;

typedef struct {
    uint8_t length;
    uint8_t data[TWAI_FRAME_MAX_LEN];
} isotp_rx_packet_t;

struct esp_isotp_link_t {
    IsoTpLink link;
    twai_node_handle_t twai_node;
    uint8_t *tx_buffer;
    uint8_t *rx_buffer;
    QueueHandle_t rx_queue;
    QueueHandle_t free_tx_slots;
    SemaphoreHandle_t link_mutex;
    portMUX_TYPE stats_mux;
    esp_isotp_diagnostics_t stats;
    int64_t last_poll_us;
    isotp_tx_slot_t tx_slots[ISOTP_TX_SLOT_COUNT];
};

static void stats_add(esp_isotp_handle_t handle, uint32_t *field)
{
    portENTER_CRITICAL_SAFE(&handle->stats_mux);
    ++*field;
    portEXIT_CRITICAL_SAFE(&handle->stats_mux);
}

/* ISR only copies raw data. ISO-TP state is exclusively touched under link_mutex. */
static bool esp_isotp_rx_callback(twai_node_handle_t node,
                                 const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    (void)edata;
    esp_isotp_handle_t handle = user_ctx;
    isotp_rx_packet_t packet = {0};
    twai_frame_t frame = {.buffer = packet.data, .buffer_len = sizeof(packet.data)};
    BaseType_t wake = pdFALSE;
    if (twai_node_receive_from_isr(node, &frame) != ESP_OK) return false;
    stats_add(handle, &handle->stats.can_rx_frames);
    if (frame.header.ide || frame.header.rtr || frame.header.fdf ||
        frame.header.id != handle->link.receive_arbitration_id) return false;
    stats_add(handle, &handle->stats.matched_rx_frames);
    size_t length = twaifd_dlc2len(frame.header.dlc);
    if (length == 0 || length > sizeof(packet.data)) return false;
    packet.length = (uint8_t)length;
    if (xQueueSendFromISR(handle->rx_queue, &packet, &wake) != pdTRUE)
        stats_add(handle, &handle->stats.rx_queue_drops);
    return wake == pdTRUE;
}

static bool esp_isotp_tx_callback(twai_node_handle_t node,
                                 const twai_tx_done_event_data_t *edata, void *user_ctx)
{
    (void)node;
    esp_isotp_handle_t handle = user_ctx;
    if (edata == NULL || edata->done_tx_frame == NULL) return false;
    /* frame is the first member; slots remain allocated until transport deletion. */
    isotp_tx_slot_t *slot = (isotp_tx_slot_t *)edata->done_tx_frame;
    if (!edata->is_tx_success) stats_add(handle, &handle->stats.tx_failed_frames);
    BaseType_t wake = pdFALSE;
    xQueueSendFromISR(handle->free_tx_slots, &slot, &wake);
    return wake == pdTRUE;
}

uint32_t isotp_user_get_us(void)
{
    return (uint32_t)esp_timer_get_time();
}

int isotp_user_send_can(uint32_t arbitration_id, const uint8_t *data,
                       uint8_t size, void *user_data)
{
    esp_isotp_handle_t handle = user_data;
    if (handle == NULL || data == NULL || size == 0 || size > TWAI_FRAME_MAX_LEN)
        return ISOTP_RET_ERROR;
    isotp_tx_slot_t *slot = NULL;
    if (xQueueReceive(handle->free_tx_slots, &slot, 0) != pdTRUE) {
        stats_add(handle, &handle->stats.tx_submit_failures);
        return ISOTP_RET_NOSPACE;
    }
    memset(&slot->frame, 0, sizeof(slot->frame));
    memcpy(slot->data, data, size);
    slot->frame.header.id = arbitration_id;
    slot->frame.buffer = slot->data;
    slot->frame.buffer_len = size;
    esp_err_t ret = twai_node_transmit(handle->twai_node, &slot->frame, 0);
    if (ret != ESP_OK) {
        xQueueSend(handle->free_tx_slots, &slot, 0);
        stats_add(handle, &handle->stats.tx_submit_failures);
        return ret == ESP_ERR_TIMEOUT ? ISOTP_RET_NOSPACE : ISOTP_RET_ERROR;
    }
    /* Only TX-done can recycle the descriptor and payload after successful submit. */
    return ISOTP_RET_OK;
}

void isotp_user_debug(const char *message, ...)
{
    va_list args;
    va_start(args, message);
    esp_log_writev(ESP_LOG_DEBUG, "isotp_c", message, args);
    va_end(args);
}

static void free_transport(esp_isotp_handle_t handle)
{
    if (handle->rx_queue) vQueueDelete(handle->rx_queue);
    if (handle->free_tx_slots) vQueueDelete(handle->free_tx_slots);
    if (handle->link_mutex) vSemaphoreDelete(handle->link_mutex);
    free(handle->rx_buffer);
    free(handle->tx_buffer);
    free(handle);
}

esp_err_t esp_isotp_new_transport(twai_node_handle_t node,
                                 const esp_isotp_config_t *config,
                                 esp_isotp_handle_t *out_handle)
{
    if (!node || !config || !out_handle || !config->rx_buffer_size || !config->tx_buffer_size)
        return ESP_ERR_INVALID_ARG;
    *out_handle = NULL;
    esp_isotp_handle_t handle = heap_caps_calloc(1, sizeof(*handle), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!handle) return ESP_ERR_NO_MEM;
    handle->stats_mux = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    handle->twai_node = node;
    handle->tx_buffer = malloc(config->tx_buffer_size);
    handle->rx_buffer = malloc(config->rx_buffer_size);
    handle->rx_queue = xQueueCreate(ISOTP_RX_QUEUE_DEPTH, sizeof(isotp_rx_packet_t));
    handle->free_tx_slots = xQueueCreate(ISOTP_TX_SLOT_COUNT, sizeof(isotp_tx_slot_t *));
    handle->link_mutex = xSemaphoreCreateMutex();
    if (!handle->tx_buffer || !handle->rx_buffer || !handle->rx_queue ||
        !handle->free_tx_slots || !handle->link_mutex) {
        free_transport(handle);
        return ESP_ERR_NO_MEM;
    }
    for (unsigned i = 0; i < ISOTP_TX_SLOT_COUNT; ++i) {
        isotp_tx_slot_t *slot = &handle->tx_slots[i];
        xQueueSend(handle->free_tx_slots, &slot, 0);
    }
    isotp_init_link(&handle->link, config->tx_id, handle->tx_buffer,
                    config->tx_buffer_size, handle->rx_buffer, config->rx_buffer_size);
    handle->link.receive_arbitration_id = config->rx_id;
    handle->link.user_send_can_arg = handle;
    twai_event_callbacks_t callbacks = {
        .on_rx_done = esp_isotp_rx_callback,
        .on_tx_done = esp_isotp_tx_callback,
    };
    esp_err_t ret = twai_node_register_event_callbacks(node, &callbacks, handle);
    if (ret != ESP_OK) {
        free_transport(handle);
        return ret;
    }
    ret = twai_node_enable(node);
    if (ret != ESP_OK) {
        twai_event_callbacks_t empty = {0};
        twai_node_register_event_callbacks(node, &empty, NULL);
        free_transport(handle);
        return ret;
    }
    *out_handle = handle;
    return ESP_OK;
}

esp_err_t esp_isotp_poll(esp_isotp_handle_t handle)
{
    if (!handle) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(handle->link_mutex, portMAX_DELAY);
    int64_t now = esp_timer_get_time();
    if (handle->last_poll_us) {
        uint32_t gap = (uint32_t)(now - handle->last_poll_us);
        if (gap > handle->stats.max_poll_gap_us) handle->stats.max_poll_gap_us = gap;
    }
    handle->last_poll_us = now;
    isotp_rx_packet_t packet;
    /* Preserve a complete message until Receive consumes it; don't overwrite it. */
    for (unsigned budget = 0; budget < ISOTP_RX_QUEUE_DEPTH &&
         handle->link.receive_status != ISOTP_RECEIVE_STATUS_FULL; ++budget) {
        if (xQueueReceive(handle->rx_queue, &packet, 0) != pdTRUE) break;
        isotp_on_can_message(&handle->link, packet.data, packet.length);
    }
    isotp_poll(&handle->link);
    xSemaphoreGive(handle->link_mutex);
    return ESP_OK;
}

esp_err_t esp_isotp_send(esp_isotp_handle_t handle, const uint8_t *data, uint32_t size)
{
    if (!handle || !data || !size) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(handle->link_mutex, portMAX_DELAY);
    int ret = isotp_send(&handle->link, data, size);
    xSemaphoreGive(handle->link_mutex);
    if (ret == ISOTP_RET_OK) return ESP_OK;
    if (ret == ISOTP_RET_INPROGRESS) return ESP_ERR_NOT_FINISHED;
    if (ret == ISOTP_RET_OVERFLOW) return ESP_ERR_NO_MEM;
    if (ret == ISOTP_RET_NOSPACE) return ESP_ERR_TIMEOUT;
    return ESP_FAIL;
}

esp_err_t esp_isotp_receive(esp_isotp_handle_t handle, uint8_t *data, uint32_t size,
                          uint32_t *received_size)
{
    if (!handle || !data || !size || !received_size) return ESP_ERR_INVALID_ARG;
    *received_size = 0;
    xSemaphoreTake(handle->link_mutex, portMAX_DELAY);
    esp_err_t ret;
    if (handle->link.receive_status != ISOTP_RECEIVE_STATUS_FULL) {
        ret = ESP_ERR_NOT_FOUND;
    } else if (handle->link.receive_size > size) {
        /* Reject the complete oversized message, never present a truncated success. */
        handle->link.receive_status = ISOTP_RECEIVE_STATUS_IDLE;
        ret = ESP_ERR_INVALID_SIZE;
    } else {
        ret = isotp_receive(&handle->link, data, size, received_size) == ISOTP_RET_OK ? ESP_OK : ESP_FAIL;
        if (ret == ESP_OK) ++handle->stats.complete_messages;
    }
    xSemaphoreGive(handle->link_mutex);
    return ret;
}

esp_err_t esp_isotp_get_diagnostics(esp_isotp_handle_t handle, esp_isotp_diagnostics_t *out)
{
    if (!handle || !out) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(handle->link_mutex, portMAX_DELAY);
    portENTER_CRITICAL_SAFE(&handle->stats_mux);
    *out = handle->stats;
    portEXIT_CRITICAL_SAFE(&handle->stats_mux);
    out->receive_status = handle->link.receive_status;
    out->receive_result = handle->link.receive_protocol_result;
    out->pending_rx_frames = uxQueueMessagesWaiting(handle->rx_queue);
    xSemaphoreGive(handle->link_mutex);
    return ESP_OK;
}

esp_err_t esp_isotp_delete(esp_isotp_handle_t handle)
{
    if (!handle) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(handle->link_mutex, portMAX_DELAY);
    /* Never free memory that TWAI may still reference, including during bus-off. */
    esp_err_t ret = twai_node_transmit_wait_all_done(handle->twai_node, 100);
    if (ret != ESP_OK) {
        xSemaphoreGive(handle->link_mutex);
        return ret;
    }
    ret = twai_node_disable(handle->twai_node);
    if (ret != ESP_OK) {
        xSemaphoreGive(handle->link_mutex);
        return ret;
    }
    twai_event_callbacks_t empty = {0};
    ret = twai_node_register_event_callbacks(handle->twai_node, &empty, NULL);
    if (ret != ESP_OK) {
        xSemaphoreGive(handle->link_mutex);
        return ret;
    }
    isotp_destroy_link(&handle->link);
    xSemaphoreGive(handle->link_mutex);
    free_transport(handle);
    return ESP_OK;
}
