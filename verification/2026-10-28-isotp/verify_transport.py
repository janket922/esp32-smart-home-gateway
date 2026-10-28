"""Native tests of the actual local adapter and upstream isotp-c state machine.

The fake asynchronous driver retains pointers and completes sends only on request.
This is a protocol/lifetime regression test, not a hardware load test.
"""
import ctypes as C
import json
import os
from pathlib import Path
import re
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
BUILD = HERE / 'build'
BUILD.mkdir(exist_ok=True)
COMPONENT = ROOT / 'Components/ThirdParty/esp_isotp'
config = '''
#define ISOTPC_CONFIG_H
#define ISO_TP_DEFAULT_BLOCK_SIZE 8
#define ISO_TP_DEFAULT_ST_MIN_US 1000
#define ISO_TP_MAX_WFT_NUMBER 1
#define ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US 100000
#define ISO_TP_USER_SEND_CAN_ARG
#define ISO_TP_TRANSMIT_COMPLETE_CALLBACK
#define ISO_TP_RECEIVE_COMPLETE_CALLBACK
'''
(BUILD / 'host_config.h').write_text(config)
prelude = r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include "isotp.h"
typedef int esp_err_t;
typedef unsigned int TickType_t;
typedef int BaseType_t;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL_SAFE(x) ((void)(x))
#define portEXIT_CRITICAL_SAFE(x) ((void)(x))
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 257
#define ESP_ERR_INVALID_ARG 258
#define ESP_ERR_INVALID_STATE 259
#define ESP_ERR_INVALID_SIZE 260
#define ESP_ERR_NOT_FOUND 261
#define ESP_ERR_NOT_FINISHED 268
#define ESP_ERR_TIMEOUT 263
#define ESP_LOG_DEBUG 4
#define TWAI_FRAME_MAX_LEN 8
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define pdFALSE 0
#define pdTRUE 1
#define portMAX_DELAY 0xffffffffU
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
static int in_isr;
typedef struct {unsigned capacity,size,count,head,tail;unsigned char* data;} Queue;
typedef Queue* QueueHandle_t;
typedef int* SemaphoreHandle_t;
QueueHandle_t xQueueCreate(unsigned n,unsigned size){Queue* q=calloc(1,sizeof(*q));q->capacity=n;q->size=size;q->data=calloc(n,size);return q;}
int xQueueSend(Queue* q,const void* p,unsigned wait){assert(wait==0);if(q->count==q->capacity)return 0;memcpy(q->data+q->tail*q->size,p,q->size);q->tail=(q->tail+1)%q->capacity;q->count++;return 1;}
int xQueueReceive(Queue* q,void* p,unsigned wait){assert(wait==0);if(!q->count)return 0;memcpy(p,q->data+q->head*q->size,q->size);q->head=(q->head+1)%q->capacity;q->count--;return 1;}
int xQueueSendFromISR(Queue* q,const void* p,int* wake){assert(in_isr);*wake=0;return xQueueSend(q,p,0);}
unsigned uxQueueMessagesWaiting(Queue* q){return q->count;}
void vQueueDelete(Queue* q){free(q->data);free(q);}
SemaphoreHandle_t xSemaphoreCreateMutex(void){return calloc(1,sizeof(int));}
int xSemaphoreTake(int* m,unsigned wait){(void)wait;assert(!in_isr && *m==0);*m=1;return 1;}
int xSemaphoreGive(int* m){assert(!in_isr && *m==1);*m=0;return 1;}
void vSemaphoreDelete(int* m){assert(!*m);free(m);}
void* heap_caps_calloc(size_t n,size_t s,int flags){(void)flags;return calloc(n,s);}
typedef void* twai_node_handle_t;
typedef struct {uint32_t id;bool ide,rtr,fdf;uint8_t dlc;} Header;
typedef struct {Header header;uint8_t* buffer;size_t buffer_len;} twai_frame_t;
typedef struct {int unused;} twai_rx_done_event_data_t;
typedef struct {bool is_tx_success;const twai_frame_t* done_tx_frame;} twai_tx_done_event_data_t;
typedef struct {
    bool (*on_rx_done)(void*,const twai_rx_done_event_data_t*,void*);
    bool (*on_tx_done)(void*,const twai_tx_done_event_data_t*,void*);
} twai_event_callbacks_t;
static twai_event_callbacks_t callbacks;
static void* callback_arg;
static const twai_frame_t* pending[64];
static unsigned pending_count;
static int submit_error,wait_error,disable_calls;
static Header incoming_header;
static uint8_t incoming_data[8];
static int64_t clock_us=1;
int64_t esp_timer_get_time(void){return clock_us;}
void esp_log_writev(int level,const char* tag,const char* message,va_list args){(void)level;(void)tag;(void)message;(void)args;}
unsigned twaifd_dlc2len(unsigned dlc){return dlc<=8?dlc:64;}
int twai_node_transmit(void* node,const twai_frame_t* frame,int timeout){(void)node;(void)timeout;if(submit_error)return submit_error;assert(pending_count<64);pending[pending_count++]=frame;return 0;}
int twai_node_register_event_callbacks(void* node,const twai_event_callbacks_t* cb,void* arg){(void)node;callbacks=*cb;callback_arg=arg;return 0;}
int twai_node_enable(void* node){(void)node;return 0;}
int twai_node_disable(void* node){(void)node;disable_calls++;return 0;}
int twai_node_transmit_wait_all_done(void* node,int wait){(void)node;(void)wait;return wait_error?wait_error:(pending_count?ESP_ERR_TIMEOUT:0);}
int twai_node_receive_from_isr(void* node,twai_frame_t* frame){(void)node;assert(in_isr);frame->header=incoming_header;memcpy(frame->buffer,incoming_data,8);return 0;}
'''

def strip(text):
    return re.sub(r'^\s*#include[^\n]*', '', text, flags=re.M)

source = prelude + strip((COMPONENT / 'inc/esp_isotp.h').read_text(encoding='utf-8'))
source += strip((COMPONENT / 'src/esp_isotp.c').read_text(encoding='utf-8-sig'))
service = (ROOT/'Components/Svc/Svc_Isotp/Svc_Isotp.c').read_bytes().decode('gb18030')
recovery = service[service.index('uint8_t Svc_Isotp_TwaiError_Detect(void)'):service.index('void Svc_Isotp_Log_Diagnostics(void)')]
source += r'''
#define TWAI_ERROR_BUS_OFF 3
typedef struct {int state;} twai_node_status_t;
static twai_node_handle_t node=(void*)1;
static bool recovery_requested;
static int fake_bus_state,recover_calls;
int twai_node_get_info(void* n,twai_node_status_t* s,void* record){(void)n;(void)record;s->state=fake_bus_state;return 0;}
int twai_node_recover(void* n){(void)n;recover_calls++;return 0;}
'''+recovery
sensor = (ROOT/'Components/App/App_Sensor_Sync/App_Sensor_Sync.c').read_text(encoding='utf-8-sig')
sensor_send = sensor[sensor.index('void App_Sensor_Sync_Send('):sensor.index('esp_err_t App_Sensor_Sync_Receive(')]
source += r'''
#define ESP_LOGE(...) ((void)0)
#define pdMS_TO_TICKS(x) (x)
typedef struct {struct {float temperature,humidity;} aht20_data;uint16_t light;} Sensor_data_t;
static QueueHandle_t App_Sensor_Sync_Queue_Handle;
TickType_t xTaskGetTickCount(void){return (TickType_t)(clock_us/1000);}
'''+sensor_send
source += r'''
#define EXPORT __declspec(dllexport)
static esp_isotp_handle_t transport;
EXPORT int create(void){pending_count=0;submit_error=wait_error=disable_calls=0;clock_us=1;esp_isotp_config_t cfg={.tx_id=0x98,.rx_id=0x717,.tx_buffer_size=512,.rx_buffer_size=512};return esp_isotp_new_transport((void*)1,&cfg,&transport);}
EXPORT void complete_one(int success){assert(pending_count);twai_tx_done_event_data_t ev={success!=0,pending[0]};for(unsigned i=1;i<pending_count;i++)pending[i-1]=pending[i];pending_count--;in_isr=1;callbacks.on_tx_done((void*)1,&ev,callback_arg);in_isr=0;}
EXPORT int destroy(void){return esp_isotp_delete(transport);}
EXPORT int submit(const uint8_t* p,unsigned n){return isotp_user_send_can(0x98,p,(uint8_t)n,transport);}
EXPORT int tx_byte(unsigned frame,unsigned offset){assert(frame<pending_count);return pending[frame]->buffer[offset];}
EXPORT int tx_id(unsigned frame){assert(frame<pending_count);return pending[frame]->header.id;}
EXPORT unsigned tx_count(void){return pending_count;}
EXPORT void set_error(int value){submit_error=value;}
EXPORT void set_wait_error(int value){wait_error=value;}
EXPORT void advance(unsigned us){clock_us+=us;}
EXPORT void feed(unsigned id,const uint8_t* p,unsigned dlc){incoming_header=(Header){.id=id,.dlc=(uint8_t)dlc};memset(incoming_data,0xA5,8);memcpy(incoming_data,p,dlc<=8?dlc:8);in_isr=1;callbacks.on_rx_done((void*)1,NULL,callback_arg);in_isr=0;}
EXPORT int poll(void){return esp_isotp_poll(transport);}
EXPORT int receive(uint8_t* p,unsigned size,unsigned* actual){return esp_isotp_receive(transport,p,size,actual);}
EXPORT int stats(esp_isotp_diagnostics_t* out){return esp_isotp_get_diagnostics(transport,out);}
EXPORT unsigned free_slots(void){return uxQueueMessagesWaiting(transport->free_tx_slots);}
EXPORT int disabled(void){return disable_calls;}
EXPORT int recovery_check(int state){fake_bus_state=state;return Svc_Isotp_TwaiError_Detect();}
EXPORT int recovery_count(void){return recover_calls;}
EXPORT int sensor_full_queue(void){App_Sensor_Sync_Queue_Handle=xQueueCreate(12,sizeof(Sensor_data_t));Sensor_data_t data={0};for(int i=0;i<12;i++)xQueueSend(App_Sensor_Sync_Queue_Handle,&data,0);App_Sensor_Sync_Send(&data);int count=uxQueueMessagesWaiting(App_Sensor_Sync_Queue_Handle);vQueueDelete(App_Sensor_Sync_Queue_Handle);App_Sensor_Sync_Queue_Handle=NULL;return count;}
'''
(BUILD / 'transport_host.c').write_text(source, encoding='utf-8')
vc = Path(r'D:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207')
sdk = Path(r'D:/Windows Kits/10')
env = dict(os.environ)
env['INCLUDE'] = ';'.join(map(str, [vc/'include', sdk/'Include/10.0.26100.0/ucrt', sdk/'Include/10.0.26100.0/shared', sdk/'Include/10.0.26100.0/um']))
env['LIB'] = ';'.join(map(str, [vc/'lib/x64', sdk/'Lib/10.0.26100.0/ucrt/x64', sdk/'Lib/10.0.26100.0/um/x64']))
command = [str(vc/'bin/Hostx64/x64/cl.exe'), '/nologo', '/utf-8', '/LD', '/MT', '/Od', '/std:c11',
           '/FI'+str(BUILD/'host_config.h'), '/I'+str(COMPONENT/'isotp-c'),
           str(BUILD/'transport_host.c'), str(COMPONENT/'isotp-c/isotp.c'),
           '/Fe:'+str(BUILD/'transport.dll')]
result = subprocess.run(command, cwd=BUILD, env=env, capture_output=True, text=True, encoding='utf-8', errors='replace')
if result.returncode:
    raise RuntimeError(result.stdout + result.stderr)
dll = C.CDLL(str(BUILD/'transport.dll'))
dll.submit.argtypes = [C.c_void_p, C.c_uint]
dll.feed.argtypes = [C.c_uint, C.c_void_p, C.c_uint]
dll.receive.argtypes = [C.c_void_p, C.c_uint, C.POINTER(C.c_uint)]
class Stats(C.Structure):
    _fields_ = [(n, C.c_uint32) for n in ('can_rx_frames','matched_rx_frames','rx_queue_drops','tx_submit_failures','tx_failed_frames','complete_messages','max_poll_gap_us','pending_rx_frames')]+[('receive_status',C.c_int),('receive_result',C.c_int)]
dll.stats.argtypes = [C.POINTER(Stats)]
results = []

def record(name, passed):
    results.append({'name': name, 'passed': bool(passed)})

def finish():
    while dll.tx_count(): dll.complete_one(1)
    assert dll.destroy() == 0

def feed(data, ident=0x717):
    dll.feed(ident, C.create_string_buffer(bytes(data)), len(data))

assert dll.create() == 0
original = C.create_string_buffer(bytes([0x30,8,1]))
record('submit queues async frame', dll.submit(original,3) == 0 and dll.tx_count()==1)
original[0] = b'\xFF'
record('driver retains stable descriptor and copied payload', dll.tx_byte(0,0)==0x30 and dll.tx_id(0)==0x98)
record('slot not recycled before TX done', dll.free_slots()==8)
for i in range(8): assert dll.submit(C.create_string_buffer(bytes([i,1,2])),3)==0
record('pending descriptors do not alias', dll.tx_byte(0,0)==0x30 and all(dll.tx_byte(i+1,0)==i for i in range(8)))
record('pool exhaustion fails without overwriting queued frame', dll.submit(original,3)==-8 and dll.tx_byte(0,0)==0x30)
dll.complete_one(0)
diag=Stats();dll.stats(C.byref(diag))
record('TX done recycles slot and records failure', dll.free_slots()==1 and diag.tx_failed_frames==1)
record('recycled slot reusable', dll.submit(original,3)==0)
record('delete preserves in-flight memory', dll.destroy()==263 and dll.disabled()==0)
finish()
assert dll.create()==0
dll.set_error(263)
record('driver queue rejection returns slot', dll.submit(original,3)==-8 and dll.free_slots()==9)
dll.set_error(0)
record('invalid TX lengths rejected', dll.submit(original,0)==-1 and dll.submit(original,9)==-1)
finish()
assert dll.create()==0
feed([0x10,0x0C,1,2,3,4,5,6])
out=C.create_string_buffer(512);actual=C.c_uint(123)
record('ISR enqueues without touching ISO-TP state', dll.receive(out,512,C.byref(actual))==261 and actual.value==0 and dll.tx_count()==0)
dll.poll()
record('poll emits valid persistent flow control', dll.tx_count()==1 and dll.tx_byte(0,0)==0x30 and dll.tx_id(0)==0x98)
feed([0x21,7,8,9,10,11,12,0]);dll.poll()
record('multi-frame sensor payload reassembles', dll.receive(out,512,C.byref(actual))==0 and actual.value==12 and out.raw[:12]==bytes(range(1,13)))
feed([0x01,42],0x123);dll.poll()
record('unmatched CAN ID not delivered', dll.receive(out,512,C.byref(actual))==261)
feed([0x07]);dll.poll()
record('DLC bounds reject truncated single frame', dll.receive(out,512,C.byref(actual))==261)
feed([0x03,5,6,7]);feed([0x02,8,9]);dll.poll()
first=dll.receive(out,512,C.byref(actual))==0 and out.raw[:3]==bytes([5,6,7]);dll.poll()
record('complete message preserved before next queued message', first and dll.receive(out,512,C.byref(actual))==0 and actual.value==2 and out.raw[:2]==bytes([8,9]))
feed([0x03,1,2,3]);dll.poll()
record('undersized output rejected instead of truncated success', dll.receive(out,2,C.byref(actual))==260 and actual.value==0)
dll.advance(120000);dll.poll();diag=Stats();dll.stats(C.byref(diag))
record('poll gap diagnostic records scheduling delay', diag.max_poll_gap_us>=120000)
finish()
assert dll.create()==0
feed([0x10,0x0C,1,2,3,4,5,6]);dll.poll();dll.advance(120000);dll.poll()
diag=Stats();dll.stats(C.byref(diag))
record('incomplete ISO-TP receive times out to idle', diag.receive_status==0)
feed([0x10,0x0C,1,2,3,4,5,6]);dll.poll();feed([0x21,7,8,9,10,11,12,0]);dll.poll()
record('next complete message recovers after protocol timeout', dll.receive(out,512,C.byref(actual))==0 and actual.value==12)
finish()
assert dll.create()==0
for _ in range(40):feed([0x01,1])
diag=Stats();dll.stats(C.byref(diag))
record('bounded RX queue counts overflow', diag.can_rx_frames==40 and diag.matched_rx_frames==40 and diag.rx_queue_drops==8 and diag.pending_rx_frames==32)
finish()
record('sensor full queue never waits behind MQTT', dll.sensor_full_queue()==12)
record('bus-off initiates recovery', dll.recovery_check(3)==1 and dll.recovery_count()==1)
record('ongoing recovery is not restarted every poll', dll.recovery_check(3)==1 and dll.recovery_count()==1)
record('active CAN clears recovery state', dll.recovery_check(0)==0)
record('later bus-off can recover again', dll.recovery_check(3)==1 and dll.recovery_count()==2)
summary={'scope':'Current local C adapter plus actual upstream isotp-c, mocked RTOS and deferred-pointer TWAI driver. Single host thread; no hardware or real ISR concurrency.',
         'passed':sum(r['passed'] for r in results),'failed':sum(not r['passed'] for r in results),'checks':results}
(HERE/'tests.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
print(json.dumps(summary,indent=2))
raise SystemExit(bool(summary['failed']))
