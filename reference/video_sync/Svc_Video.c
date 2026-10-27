#include "Svc_Video.h"
#include "Video.h"
#include "esp_heap_caps.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#define LOCK_WAIT pdMS_TO_TICKS(2000)
#define CLOSE_WAIT pdMS_TO_TICKS(2000)
#define POLL_STEP ((pdMS_TO_TICKS(10) > 0) ? pdMS_TO_TICKS(10) : 1)

/* Lock order: io_lock -> state_lock. Release/Hold only need state_lock.
 * Never hold state_lock while waiting for a frame or for its owner to finish. */
static SemaphoreHandle_t state_lock, io_lock;
static QueueHandle_t free_queue, ready_queue;
static svc_frame_packet_t pool[SVC_VIDEO_POOL_SIZE];
static svc_frame_packet_t *held_frame;
static TaskHandle_t frame_owner;
static bool initialized, pool_initialized, ready, started, closing = true;

static void LockState(void) { xSemaphoreTake(state_lock, portMAX_DELAY); }
static void UnlockState(void) { xSemaphoreGive(state_lock); }

/* Called only by the lifecycle owner, with io_lock held and admission closed. */
static void PoolDestroy(void)
{
    for (int i = 0; i < SVC_VIDEO_POOL_SIZE; ++i) {
        heap_caps_free(pool[i].payload);
        pool[i].payload = NULL;
        pool[i].length = 0;
    }
    if (free_queue) vQueueDelete(free_queue);
    if (ready_queue) vQueueDelete(ready_queue);
    free_queue = ready_queue = NULL;
    pool_initialized = false;
}

static esp_err_t PoolCreate(void)
{
    if (pool_initialized) return ESP_OK;
    free_queue = xQueueCreate(SVC_VIDEO_POOL_SIZE, sizeof(svc_frame_packet_t *));
    ready_queue = xQueueCreate(SVC_VIDEO_POOL_SIZE, sizeof(svc_frame_packet_t *));
    if (!free_queue || !ready_queue) goto fail;
    for (int i = 0; i < SVC_VIDEO_POOL_SIZE; ++i) {
        pool[i].payload = heap_caps_malloc(SVC_VIDEO_MAX_FRAME_SIZE,
                                          MALLOC_CAP_SPIRAM);
        if (!pool[i].payload) goto fail;
        pool[i].length = 0;
        svc_frame_packet_t *p = &pool[i];
        if (xQueueSend(free_queue, &p, 0) != pdTRUE) goto fail;
    }
    pool_initialized = true;
    return ESP_OK;
fail:
    PoolDestroy();
    return ESP_ERR_NO_MEM;
}

/* Caller holds io_lock. No other capture or lifecycle operation can run.
 * Timeout keeps queues and payloads intact; Release remains available. */
static esp_err_t CloseLocked(void)
{
    LockState();
    closing = true;
    bool was_started = started;
    bool was_ready = ready;
    UnlockState();

    if (was_started) {
        esp_err_t ret = bsp_video_stop();
        if (ret != ESP_OK) return ret;
        LockState();
        started = false;
        UnlockState();
    }

    TickType_t begin = xTaskGetTickCount();
    for (;;) {
        LockState();
        bool idle = held_frame == NULL;
        bool own_frame = !idle && frame_owner == xTaskGetCurrentTaskHandle();
        UnlockState();
        if (idle) break;
        if (own_frame) return ESP_ERR_INVALID_STATE;
        if ((TickType_t)(xTaskGetTickCount() - begin) >= CLOSE_WAIT)
            return ESP_ERR_TIMEOUT;
        vTaskDelay(POLL_STEP);
    }

    /* Drop queued old frames. The uploader has returned its only held frame. */
    LockState();
    svc_frame_packet_t *p;
    esp_err_t ret = ESP_OK;
    while (ready_queue && xQueueReceive(ready_queue, &p, 0) == pdTRUE) {
        p->length = 0;
        if (xQueueSend(free_queue, &p, 0) != pdTRUE) {
            ret = ESP_FAIL;
            break;
        }
    }
    UnlockState();
    if (ret != ESP_OK) return ret;
    if (was_ready) {
        ret = bsp_video_off();
        if (ret != ESP_OK) return ret;
        LockState();
        ready = false;
        UnlockState();
    }
    return ESP_OK;
}

esp_err_t Svc_Video_Init(void)
{
    /* The first call is single-threaded, before the application tasks start. */
    if (!state_lock) {
        state_lock = xSemaphoreCreateMutex();
        io_lock = xSemaphoreCreateMutex();
        if (!state_lock || !io_lock) {
            if (state_lock) vSemaphoreDelete(state_lock);
            if (io_lock) vSemaphoreDelete(io_lock);
            state_lock = io_lock = NULL;
            return ESP_ERR_NO_MEM;
        }
    }
    if (xSemaphoreTake(io_lock, LOCK_WAIT) != pdTRUE) return ESP_ERR_TIMEOUT;
    LockState();
    bool done = initialized;
    UnlockState();
    esp_err_t ret = ESP_OK;
    if (!done) {
        ret = bsp_video_init();
        if (ret == ESP_OK) {
            ret = PoolCreate();
            if (ret != ESP_OK) bsp_video_deinit();
        }
        LockState();
        initialized = ret == ESP_OK;
        ready = started = false;
        closing = true;  /* Ctrl(video_on) opens admission. */
        UnlockState();
    }
    xSemaphoreGive(io_lock);
    return ret;
}

esp_err_t Svc_Video_Ctrl(svc_video_state_t state)
{
    if (state != video_on && state != video_off) return ESP_ERR_INVALID_ARG;
    if (!io_lock) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(io_lock, LOCK_WAIT) != pdTRUE) return ESP_ERR_TIMEOUT;
    LockState();
    bool exists = initialized;
    bool already_ready = ready;
    UnlockState();
    esp_err_t ret;
    if (state == video_off) {
        ret = CloseLocked();
    } else if (!exists) {
        ret = ESP_ERR_INVALID_STATE;
    } else {
        ret = already_ready ? ESP_OK : bsp_video_ready();
        if (ret == ESP_OK) {
            LockState();
            ready = true;
            closing = false;
            UnlockState();
        }
    }
    xSemaphoreGive(io_lock);
    return ret;
}

esp_err_t Svc_Video_Deinit(void)
{
    if (!io_lock) return ESP_OK;
    if (xSemaphoreTake(io_lock, LOCK_WAIT) != pdTRUE) return ESP_ERR_TIMEOUT;
    esp_err_t ret = CloseLocked();
    if (ret == ESP_OK) {
        LockState();
        bool exists = initialized;
        UnlockState();
        if (exists) ret = bsp_video_deinit();
        if (ret == ESP_OK) {
            LockState();
            initialized = false;
            UnlockState();
            PoolDestroy();
        }
    }
    /* Keep synchronization objects alive across Deinit/Init. */
    xSemaphoreGive(io_lock);
    return ret;
}

esp_err_t Svc_Video_Reset(void)
{
    if (!io_lock) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(io_lock, LOCK_WAIT) != pdTRUE) return ESP_ERR_TIMEOUT;
    LockState();
    bool exists = initialized;
    UnlockState();
    esp_err_t ret = exists ? CloseLocked() : ESP_ERR_INVALID_STATE;
    if (ret == ESP_OK) ret = bsp_video_deinit();
    if (ret == ESP_OK) {
        LockState();
        initialized = false;
        UnlockState();
        ret = bsp_video_init();
        LockState();
        initialized = ret == ESP_OK;
        UnlockState();
    }
    /* Frame pool survives Reset; callers reopen with Ctrl(video_on). */
    xSemaphoreGive(io_lock);
    return ret;
}

static esp_err_t SetStreaming(bool on)
{
    if (!io_lock) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(io_lock, LOCK_WAIT) != pdTRUE) return ESP_ERR_TIMEOUT;
    LockState();
    bool allowed = initialized && ready && !closing;
    bool old = started;
    UnlockState();
    esp_err_t ret = ESP_OK;
    if (on && !allowed) ret = ESP_ERR_INVALID_STATE;
    else if (old != on) ret = on ? bsp_video_start() : bsp_video_stop();
    if (ret == ESP_OK) {
        LockState();
        started = on;
        UnlockState();
    }
    xSemaphoreGive(io_lock);
    return ret;
}

esp_err_t Svc_Video_Start(void) { return SetStreaming(true); }
esp_err_t Svc_Video_Stop(void) { return SetStreaming(false); }

esp_err_t Svc_Video_Frame_Ready(void)
{
    if (!io_lock) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(io_lock, LOCK_WAIT) != pdTRUE) return ESP_ERR_TIMEOUT;
    LockState();
    if (!initialized || !ready || !started || closing) {
        UnlockState();
        xSemaphoreGive(io_lock);
        return ESP_ERR_INVALID_STATE;
    }
    svc_frame_packet_t *p = NULL;
    if (xQueueReceive(free_queue, &p, 0) != pdTRUE &&
        xQueueReceive(ready_queue, &p, 0) != pdTRUE) {
        UnlockState();
        xSemaphoreGive(io_lock);
        return ESP_ERR_NOT_FOUND;
    }
    UnlockState();

    bsp_frame_packet_t frame = { .payload = p->payload, .length = 0 };
    esp_err_t ret = bsp_video_get_frame(&frame);
    LockState();
    if (ret == ESP_OK && frame.length > 0 &&
        frame.length <= SVC_VIDEO_MAX_FRAME_SIZE) {
        p->length = frame.length;
        if (xQueueSend(ready_queue, &p, 0) != pdTRUE) ret = ESP_FAIL;
    } else if (ret == ESP_OK) ret = ESP_ERR_INVALID_SIZE;
    if (ret != ESP_OK) {
        p->length = 0;
        if (xQueueSend(free_queue, &p, 0) != pdTRUE) ret = ESP_FAIL;
    }
    UnlockState();
    xSemaphoreGive(io_lock);
    return ret;
}

esp_err_t Svc_Video_Frame_Hold(svc_frame_packet_t **p, TickType_t wait)
{
    if (!p) return ESP_ERR_INVALID_ARG;
    if (*p) return ESP_ERR_INVALID_STATE;
    if (!state_lock) return ESP_ERR_INVALID_STATE;
    TickType_t begin = xTaskGetTickCount();
    for (;;) {
        LockState();
        if (!initialized || !ready || closing) {
            UnlockState();
            return ESP_ERR_INVALID_STATE;
        }
        /* Nonblocking receive under the state lock: no queue waiter can
         * remain inside xQueueReceive when Deinit deletes the queue. */
        if (!held_frame && xQueueReceive(ready_queue, p, 0) == pdTRUE) {
            held_frame = *p;
            frame_owner = xTaskGetCurrentTaskHandle();
            UnlockState();
            return ESP_OK;
        }
        UnlockState();
        TickType_t elapsed = xTaskGetTickCount() - begin;
        if (wait != portMAX_DELAY && elapsed >= wait) return ESP_ERR_TIMEOUT;
        TickType_t delay = POLL_STEP;
        if (wait != portMAX_DELAY && delay > wait - elapsed) delay = wait - elapsed;
        vTaskDelay(delay);
    }
}

esp_err_t Svc_Video_Frame_Release(svc_frame_packet_t **p)
{
    if (!p || !*p) return ESP_ERR_INVALID_ARG;
    if (!state_lock) return ESP_ERR_INVALID_STATE;
    LockState();
    esp_err_t ret = ESP_ERR_INVALID_STATE;
    if (held_frame == *p && frame_owner == xTaskGetCurrentTaskHandle()) {
        if (xQueueSend(free_queue, p, 0) == pdTRUE) {
            held_frame = NULL;
            frame_owner = NULL;
            *p = NULL;
            ret = ESP_OK;
        } else ret = ESP_FAIL;
    }
    UnlockState();
    return ret;
}
