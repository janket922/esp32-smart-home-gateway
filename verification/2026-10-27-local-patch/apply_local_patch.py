"""Apply the local synchronization changes, preserving original APIs/names."""
from pathlib import Path
import hashlib
import json
import re

OUT=Path(__file__).resolve().parent
ROOT=OUT.parents[1]

def load(rel):
    p=ROOT/rel;raw=p.read_bytes()
    for enc in ('utf-8','gb18030'):
        try:text=raw.decode(enc);break
        except UnicodeDecodeError:continue
    else:raise ValueError('Unknown source encoding')
    backup=OUT/'before'/rel;backup.parent.mkdir(parents=True,exist_ok=True)
    if backup.exists():raise RuntimeError('Backup exists; patch is intentionally one-shot')
    backup.write_bytes(raw)
    return p,text.replace('\r\n','\n'),enc,raw

def replace(text,name,body):
    m=re.search(r'(?m)^(?:void|esp_err_t)\s+'+name+r'\([^\n]*\)\n\{',text)
    if not m:raise ValueError(name)
    start=m.end()-1;end=start+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[:start]+body.strip()+text[end:]

p,text,enc,raw=load('Components/Svc/Svc_Video/Svc_Video.c')
text=text.replace('#include "Svc_Video.h"','#include "Svc_Video.h"\n#include "freertos/semphr.h"',1)
anchor='static svc_frame_packet_t **holding_frame_pointer = NULL;'
text=text.replace(anchor,anchor+'''
static TaskHandle_t holding_task_handle = NULL;
static SemaphoreHandle_t video_mutex = NULL;
static SemaphoreHandle_t video_control_mutex = NULL;
static bool is_video_closing = true;

#define VIDEO_LOCK_WAIT pdMS_TO_TICKS(2000)
#define VIDEO_CLOSE_WAIT pdMS_TO_TICKS(2000)
#define VIDEO_POLL_TICKS ((pdMS_TO_TICKS(10) > 0) ? pdMS_TO_TICKS(10) : 1)

/* Lock order: control -> video. Hold/Release only take video_mutex.
 * Recursive control lock preserves the existing nested lifecycle calls. */
static bool Svc_Video_Control_Take(void)
{
    return video_control_mutex != NULL &&
        xSemaphoreTakeRecursive(video_control_mutex, VIDEO_LOCK_WAIT) == pdTRUE;
}

static void Svc_Video_Control_Give(void)
{
    xSemaphoreGiveRecursive(video_control_mutex);
}
''',1)
bodies={
'Svc_Video_Init':r'''{
	/* First Init is called by app_main before creating the application tasks. */
	if (video_mutex == NULL)
	{
		video_mutex = xSemaphoreCreateMutex();
		video_control_mutex = xSemaphoreCreateRecursiveMutex();
		if (video_mutex == NULL || video_control_mutex == NULL)
		{
			if (video_mutex) vSemaphoreDelete(video_mutex);
			if (video_control_mutex) vSemaphoreDelete(video_control_mutex);
			video_mutex = video_control_mutex = NULL;
			ESP_LOGE("Svc_Video", "Mutex create failed");
			return;
		}
	}
	if (!Svc_Video_Control_Take()) return;
	if (is_init != false) goto done;
	if (bsp_video_init() != ESP_OK) goto done;
	Svc_Buf_pool_Init();
	xSemaphoreTake(video_mutex, portMAX_DELAY);
	is_init = is_pool_init;
	is_video_closing = true;
	xSemaphoreGive(video_mutex);
	if (!is_init) bsp_video_deinit();
done:
	Svc_Video_Control_Give();
}''',
'Svc_Video_Deinit':r'''{
	if (!Svc_Video_Control_Take()) return;
	if (is_init != true) goto done;
	/* Ctrl closes admission and waits for the uploader's own Release. */
	if (Svc_Video_Ctrl(video_off) != ESP_OK)
	{
		ESP_LOGE("Svc_Video", "Close failed; frame pool retained");
		goto done;
	}
	if (bsp_video_deinit() != ESP_OK) goto done;
	xSemaphoreTake(video_mutex, portMAX_DELAY);
	is_init = false;
	is_video_start = false;
	xSemaphoreGive(video_mutex);
	Svc_Buf_pool_Deinit();
done:
	Svc_Video_Control_Give();
}''',
'Svc_Video_Reset':r'''{
	if (!Svc_Video_Control_Take()) return;
	if (!is_init || Svc_Video_Ctrl(video_off) != ESP_OK)
	{
		ESP_LOGE("Svc_Video", "Reset postponed; resources retained");
		goto done;
	}
	if (bsp_video_deinit() != ESP_OK) goto done;
	xSemaphoreTake(video_mutex, portMAX_DELAY);
	is_init = false;
	xSemaphoreGive(video_mutex);
	vTaskDelay(pdMS_TO_TICKS(1000));
	esp_err_t ret = bsp_video_init();
	xSemaphoreTake(video_mutex, portMAX_DELAY);
	is_init = ret == ESP_OK;
	xSemaphoreGive(video_mutex);
done:
	Svc_Video_Control_Give();
}''',
'Svc_Buf_pool_Init':r'''{
	if (!Svc_Video_Control_Take()) return;
	if (is_pool_init != false) goto done;
	/* Admission is closed during pool creation. */
	free_buf_queue = xQueueCreate(SVC_VIDEO_POOL_SIZE, sizeof(svc_frame_packet_t *));
	ready_buf_queue = xQueueCreate(SVC_VIDEO_POOL_SIZE, sizeof(svc_frame_packet_t *));
	if (!free_buf_queue || !ready_buf_queue) goto fail;
	for (int i = 0; i < SVC_VIDEO_POOL_SIZE; i++)
	{
		video_buffer_pool[i].payload = heap_caps_malloc(SVC_VIDEO_MAX_FRAME_SIZE, MALLOC_CAP_SPIRAM);
		video_buffer_pool[i].length = 0;
		if (!video_buffer_pool[i].payload) goto fail;
		svc_frame_packet_t *p = &video_buffer_pool[i];
		if (xQueueSend(free_buf_queue, &p, 0) != pdTRUE) goto fail;
	}
	xSemaphoreTake(video_mutex, portMAX_DELAY);
	is_pool_init = true;
	xSemaphoreGive(video_mutex);
	goto done;
fail:
	Svc_Buf_pool_Deinit();
	ESP_LOGE("Svc_Video", "Frame pool create failed");
done:
	Svc_Video_Control_Give();
}''',
'Svc_Buf_pool_Deinit':r'''{
	if (!Svc_Video_Control_Take()) return;
	xSemaphoreTake(video_mutex, portMAX_DELAY);
	/* Keep this original public helper safe when called directly. */
	if (!is_video_closing || is_frame_holding || is_video_ready || is_video_start)
	{
		xSemaphoreGive(video_mutex);
		Svc_Video_Control_Give();
		return;
	}
	for (int i = 0; i < SVC_VIDEO_POOL_SIZE; i++)
	{
		video_buffer_pool[i].length = 0;
		heap_caps_free(video_buffer_pool[i].payload);
		video_buffer_pool[i].payload = NULL;
	}
	if (free_buf_queue) vQueueDelete(free_buf_queue);
	free_buf_queue = NULL;
	if (ready_buf_queue) vQueueDelete(ready_buf_queue);
	ready_buf_queue = NULL;
	is_pool_init = false;
	xSemaphoreGive(video_mutex);
	Svc_Video_Control_Give();
}''',
'Svc_Video_Ctrl':r'''{
	if (state != video_on && state != video_off) return ESP_ERR_INVALID_ARG;
	if (!Svc_Video_Control_Take()) return ESP_ERR_TIMEOUT;
	esp_err_t ret = ESP_OK;
	switch (state)
	{
	case video_on:
		if (!is_init || !is_pool_init) { ret = ESP_ERR_INVALID_STATE; break; }
		if (is_video_ready == false) ret = bsp_video_ready();
		if (ret == ESP_OK)
		{
			xSemaphoreTake(video_mutex, portMAX_DELAY);
			is_video_ready = true;
			is_video_closing = false;
			xSemaphoreGive(video_mutex);
		}
		break;
	case video_off:
		xSemaphoreTake(video_mutex, portMAX_DELAY);
		is_video_closing = true;
		xSemaphoreGive(video_mutex);
		ret = Svc_Video_Stop();
		if (ret != ESP_OK) break;
		TickType_t begin = xTaskGetTickCount();
		while (1)
		{
			xSemaphoreTake(video_mutex, portMAX_DELAY);
			bool idle = !is_frame_holding;
			bool own_frame = !idle && holding_task_handle == xTaskGetCurrentTaskHandle();
			xSemaphoreGive(video_mutex);
			if (idle) break;
			if (own_frame) { ret = ESP_ERR_INVALID_STATE; break; }
			if ((TickType_t)(xTaskGetTickCount() - begin) >= VIDEO_CLOSE_WAIT)
			{ ret = ESP_ERR_TIMEOUT; break; }
			/* Release can take video_mutex while we wait. */
			vTaskDelay(VIDEO_POLL_TICKS);
		}
		if (ret != ESP_OK) break;
		/* Drop old queued frames, only after the uploader has finished. */
		xSemaphoreTake(video_mutex, portMAX_DELAY);
		svc_frame_packet_t *p;
		while (ready_buf_queue && xQueueReceive(ready_buf_queue, &p, 0) == pdTRUE)
		{
			p->length = 0;
			if (xQueueSend(free_buf_queue, &p, 0) != pdTRUE) { ret = ESP_FAIL; break; }
		}
		xSemaphoreGive(video_mutex);
		if (ret != ESP_OK) break;
		if (is_video_ready == true)
		{
			ret = bsp_video_off();
			if (ret == ESP_OK)
			{
				xSemaphoreTake(video_mutex, portMAX_DELAY);
				is_video_ready = false;
				xSemaphoreGive(video_mutex);
			}
		}
		break;
	default:
		ret = ESP_ERR_INVALID_ARG;
		break;
	}
	Svc_Video_Control_Give();
	return ret;
}''',
'Svc_Video_Start':r'''{
	if (!Svc_Video_Control_Take()) return ESP_ERR_TIMEOUT;
	esp_err_t ret = ESP_OK;
	if (!is_init || !is_pool_init || !is_video_ready || is_video_closing)
		ret = ESP_ERR_INVALID_STATE;
	else if (is_video_start == false)
	{
		ret = bsp_video_start();
		if (ret == ESP_OK) is_video_start = true;
	}
	Svc_Video_Control_Give();
	return ret;
}''',
'Svc_Video_Stop':r'''{
	if (!Svc_Video_Control_Take()) return ESP_ERR_TIMEOUT;
	esp_err_t ret = ESP_OK;
	if (is_video_start == true)
	{
		ret = bsp_video_stop();
		if (ret == ESP_OK) is_video_start = false;
	}
	Svc_Video_Control_Give();
	return ret;
}''',
'Svc_Video_Frame_Ready':r'''{
	if (!Svc_Video_Control_Take()) return ESP_ERR_TIMEOUT;
	esp_err_t ret = ESP_ERR_INVALID_STATE;
	xSemaphoreTake(video_mutex, portMAX_DELAY);
	if (!is_init || !is_pool_init || !is_video_ready || !is_video_start || is_video_closing)
		goto done;
	svc_frame_packet_t *p = NULL;
	if (xQueueReceive(free_buf_queue, &p, 0) != pdTRUE)
	{
		if (xQueueReceive(ready_buf_queue, &p, 0) != pdTRUE) goto done;
	}
	xSemaphoreGive(video_mutex);
	/* The control lock keeps the camera alive during this operation. */
	ret = bsp_video_get_frame((bsp_frame_packet_t *)p);
	xSemaphoreTake(video_mutex, portMAX_DELAY);
	if (ret != ESP_OK)
	{
		p->length = 0;
		if (xQueueSend(free_buf_queue, &p, 0) != pdTRUE) ret = ESP_FAIL;
		goto done;
	}
	if (xQueueSend(ready_buf_queue, &p, 0) != pdTRUE)
	{
		p->length = 0;
		xQueueSend(free_buf_queue, &p, 0);
		ret = ESP_ERR_NOT_FINISHED;
	}
done:
	xSemaphoreGive(video_mutex);
	Svc_Video_Control_Give();
	return ret;
}''',
'Svc_Video_Frame_Hold':r'''{
	if (p == NULL) return ESP_ERR_INVALID_ARG;
	if (*p != NULL || video_mutex == NULL) return ESP_ERR_INVALID_STATE;
	TickType_t begin = xTaskGetTickCount();
	while (1)
	{
		xSemaphoreTake(video_mutex, portMAX_DELAY);
		if (!is_init || !is_pool_init || !is_video_ready || is_video_closing)
		{
			xSemaphoreGive(video_mutex);
			return ESP_ERR_INVALID_STATE;
		}
		/* Nonblocking queue access: Deinit never deletes a queue with a waiter. */
		if (is_frame_holding == false && xQueueReceive(ready_buf_queue, p, 0) == pdTRUE)
		{
			holding_frame_pointer = p;
			holding_task_handle = xTaskGetCurrentTaskHandle();
			is_frame_holding = true;
			xSemaphoreGive(video_mutex);
			return ESP_OK;
		}
		xSemaphoreGive(video_mutex);
		TickType_t elapsed = xTaskGetTickCount() - begin;
		if (xTicksToWait != portMAX_DELAY && elapsed >= xTicksToWait)
			return ESP_ERR_TIMEOUT;
		TickType_t delay = VIDEO_POLL_TICKS;
		if (xTicksToWait != portMAX_DELAY && delay > xTicksToWait - elapsed)
			delay = xTicksToWait - elapsed;
		vTaskDelay(delay);
	}
}''',
'Svc_Video_Frame_Release':r'''{
	if (p == NULL || *p == NULL) return ESP_ERR_INVALID_ARG;
	if (video_mutex == NULL) return ESP_ERR_INVALID_STATE;
	xSemaphoreTake(video_mutex, portMAX_DELAY);
	esp_err_t ret = ESP_ERR_INVALID_STATE;
	if (is_frame_holding == true && holding_frame_pointer == p &&
		holding_task_handle == xTaskGetCurrentTaskHandle())
	{
		if (xQueueSend(free_buf_queue, p, 0) == pdTRUE)
		{
			*p = NULL;
			holding_frame_pointer = NULL;
			holding_task_handle = NULL;
			is_frame_holding = false;
			ret = ESP_OK;
		}
		else ret = ESP_FAIL;
	}
	xSemaphoreGive(video_mutex);
	return ret;
}'''
}
for name,body in bodies.items():text=replace(text,name,body)
p.write_bytes(text.replace('\n','\r\n').encode(enc))

p2,text2,enc2,raw2=load('Components/App/App_Cloud_Upload/App_Cloud_Upload.c')
text2=text2.replace('''	static const char *TAG = "App_Cloud_Video_Frame_Upload";
	while (1)''','''	static const char *TAG = "App_Cloud_Video_Frame_Upload";
	if (encoded_buf == NULL)
	{
		ESP_LOGE(TAG, "Encoder buffer allocation failed");
		vTaskDelete(NULL);
		return;
	}
	while (1)''',1)
text2=text2.replace('''		Svc_Video_Frame_Hold(&frame_buf, portMAX_DELAY);''','''		esp_err_t ret = Svc_Video_Frame_Hold(&frame_buf, portMAX_DELAY);
		if (ret != ESP_OK)
		{
			/* Video is closing or unavailable; wait for a later reopen. */
			vTaskDelay(pdMS_TO_TICKS(20));
			continue;
		}''',1)
text2=text2.replace('''			Svc_Video_Frame_Release(&frame_buf);''','''			ESP_ERROR_CHECK(Svc_Video_Frame_Release(&frame_buf));''',1)
p2.write_bytes(text2.replace('\n','\r\n').encode(enc2))
manifest={'scope':'Local edits to existing functions; original public header and all other business files preserved. First Init remains before task creation.','files':[]}
for p0,old,encoding in ((p,raw,enc),(p2,raw2,enc2)):
    manifest['files'].append({'path':str(p0),'encoding':encoding,'before_sha256':hashlib.sha256(old).hexdigest(),'after_sha256':hashlib.sha256(p0.read_bytes()).hexdigest()})
(OUT/'changes.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(manifest,ensure_ascii=False,indent=2))
