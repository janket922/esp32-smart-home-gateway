"""Check selected current C function bodies on Windows with hardware/RTOS mocks.
Business files are only read. This is not an ESP firmware or concurrency test.
"""
import base64
import ctypes as C
import hashlib
import json
from pathlib import Path
import re
import subprocess

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[1]
BUILD = OUT / 'build'
BUILD.mkdir(exist_ok=True)
CL = Path(r'D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe')
LINK = CL.with_name('link.exe')
MBED_SOURCE = Path(r'D:\esp\v6.0.1\esp-idf\components\mbedtls\mbedtls\tf-psa-crypto\drivers\builtin\src\base64.c')

def read(rel):
    return (ROOT / rel).read_text(encoding='utf-8', errors='replace')

def extract_function(text, name):
    match = re.search(r'(?m)^(?:int|void|esp_err_t)\s+'+name+r'\s*\(', text)
    brace = text.index('{', match.end())
    depth, end = 1, brace+1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[match.start():end]

def no_includes(text):
    return re.sub(r'^\s*#include[^\n]*', '', text, flags=re.M)

prelude = r'''
typedef unsigned long long size_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef int bool;
typedef int esp_err_t;
typedef int TickType_t;
typedef void* TaskHandle_t;
#define true 1
#define false 0
#define NULL ((void*)0)
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 258
#define ESP_ERR_NOT_FOUND 261
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY -1
#define pdMS_TO_TICKS(x) (x)
#define MALLOC_CAP_SPIRAM 0
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define SIZE_MAX (~(size_t)0)
#define MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL -42
#define EXPORT __declspec(dllexport)
void* memcpy(void* d,const void* s,size_t n){unsigned char* a=d;const unsigned char* b=s;for(size_t i=0;i<n;i++)a[i]=b[i];return d;}
void* memset(void* d,int c,size_t n){unsigned char* a=d;for(size_t i=0;i<n;i++)a[i]=(unsigned char)c;return d;}
size_t strlen(const char* s){size_t n=0;while(s[n])n++;return n;}
typedef struct {uint8_t* payload;size_t length;} svc_frame_packet_t;
typedef svc_frame_packet_t bsp_frame_packet_t;
typedef enum {video_off=0,video_on} svc_video_state_t;
typedef struct {float temperature;float humidity;} AHT20_Data_t;
typedef struct {AHT20_Data_t aht20_data;uint16_t light;} Sensor_data_t;
typedef struct {int count,head,tail,capacity,item_size,deleted;uint8_t items[16][32];} FakeQueue;
typedef FakeQueue* QueueHandle_t;
static FakeQueue queues[16];static int queue_index;
static uint8_t fake_payloads[6][30720];static int allocation_index;
static int frame_error,capture_calls,init_error,invalid_queue_calls;
QueueHandle_t xQueueCreate(int n,int size){FakeQueue* q=&queues[queue_index++];memset(q,0,sizeof(*q));q->capacity=n;q->item_size=size;return q;}
int xQueueSend(QueueHandle_t q,const void* p,int wait){if(!q||q->deleted){invalid_queue_calls++;return 0;}if(q->count==q->capacity)return 0;memcpy(q->items[q->tail],p,q->item_size);q->tail=(q->tail+1)%q->capacity;q->count++;return 1;}
int xQueueReceive(QueueHandle_t q,void* p,int wait){if(!q||q->deleted){invalid_queue_calls++;return 0;}if(!q->count)return 0;memcpy(p,q->items[q->head],q->item_size);q->head=(q->head+1)%q->capacity;q->count--;return 1;}
void vQueueDelete(QueueHandle_t q){if(q)q->deleted=1;}
void* heap_caps_malloc(size_t n,int flags){return fake_payloads[(allocation_index++)%6];}
void heap_caps_free(void* p){}
void vTaskDelay(int ticks){}
int xTaskCreate(void(*fn)(void*),const char* name,int size,void* args,int priority,TaskHandle_t* handle){return pdPASS;}
int bsp_video_init(void){return init_error;}int bsp_video_deinit(void){return 0;}
int bsp_video_ready(void){return 0;}int bsp_video_off(void){return 0;}
int bsp_video_start(void){return 0;}int bsp_video_stop(void){return 0;}
int bsp_video_get_frame(bsp_frame_packet_t* p){capture_calls++;if(frame_error)return -1;p->length=3;p->payload[0]=(uint8_t)capture_calls;p->payload[1]=2;p->payload[2]=3;return 0;}
int Svc_Isotp_Poll(void){return -1;}
int Svc_Isotp_Receive(uint8_t* p,uint32_t n,uint32_t* actual){return -1;}
void Svc_Buf_pool_Init(void);void Svc_Buf_pool_Deinit(void);
esp_err_t Svc_Video_Ctrl(svc_video_state_t);esp_err_t Svc_Video_Frame_Release(svc_frame_packet_t**);
void App_Sensor_Sync(void*);void App_Sensor_Sync_Send(Sensor_data_t*);
unsigned char mbedtls_ct_base64_enc_char(unsigned char c){const char* table="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";return table[c];}
#define LED(x) ((void)0)
#define LED_TOGGER() ((void)0)
typedef enum {video_idle=0,video_start,video_stop,video_shutdown,video_error} video_state;
video_state current_video_state=video_idle;
uint8_t get_frame_retry_count=0;
static int check_loop_budget;
'''

# Copy numeric definitions from the current headers, not from an earlier snapshot.
defines=[]
for rel, names in [
    ('Components/Svc/Svc_Video/Svc_Video.h', ['SVC_VIDEO_POOL_SIZE','SVC_VIDEO_MAX_FRAME_SIZE']),
    ('Components/App/App_Sensor_Sync/App_Sensor_Sync.h', ['APP_SENSOR_SYNC_QUEUE_SIZE','APP_SENSOR_SYNC_TASK_SIZE','APP_SENSOR_SYNC_TASK_PRIORITY']),
    ('Components/App/App_Video_Streaming/App_Video_Streaming.h', ['VIDEO_MAX_FRAME_SIZE','VIDEO_MAX_FRAME_RATE','VIDEO_TASK_DELAY_MS','VIDEO_RESET_MAX_COUNT']),
    ('Components/Svc/Svc_Encode_Base64/Svc_Encode_Base64.h', ['MAX_ENCODER_LEN']),
]:
    for name in names:
        defines.append(re.search(r'^#define\s+'+name+r'\s+[^\n]+',read(rel),re.M).group(0).split('//')[0])

video=no_includes(read('Components/Svc/Svc_Video/Svc_Video.c'))
sensor=no_includes(read('Components/App/App_Sensor_Sync/App_Sensor_Sync.c'))
# Namespacing only; logging macros discard the TAG argument in this harness.
sensor=sensor.replace('static const char *TAG','static const char *SENSOR_TAG')
app=extract_function(read('Components/App/App_Video_Streaming/App_Video_Streaming.c'),'App_Video_Streaming')
# Bound the otherwise infinite task loop. The body of each iteration is unchanged.
app=app.replace('while (1)','while (check_loop_budget-- > 0)',1)
encoder=no_includes(read('Components/Svc/Svc_Encode_Base64/Svc_Encode_Base64.c'))
wrappers=r'''
static svc_frame_packet_t* check_held;
EXPORT void check_reset(void){queue_index=0;allocation_index=0;frame_error=0;init_error=0;capture_calls=0;invalid_queue_calls=0;is_init=0;is_pool_init=0;is_video_ready=0;is_video_start=0;is_frame_holding=0;holding_frame_pointer=0;free_buf_queue=0;ready_buf_queue=0;App_Sensor_Sync_Queue_Handle=0;current_video_state=video_idle;get_frame_retry_count=0;check_held=0;}
EXPORT void check_video_init(int fail){init_error=fail;Svc_Video_Init();}
EXPORT int check_initialized(void){return is_init;}
EXPORT int check_ready(int fail){frame_error=fail;return Svc_Video_Frame_Ready();}
EXPORT int check_free(void){return free_buf_queue?free_buf_queue->count:-1;}
EXPORT int check_queued(void){return ready_buf_queue?ready_buf_queue->count:-1;}
EXPORT int check_hold(void){return Svc_Video_Frame_Hold(&check_held,0);}
EXPORT int check_release(void){return Svc_Video_Frame_Release(&check_held);}
EXPORT size_t check_held_length(void){return check_held?check_held->length:0;}
EXPORT int check_held_byte(void){return check_held?check_held->payload[0]:-1;}
EXPORT int check_is_holding(void){return is_frame_holding;}
EXPORT int check_ctrl(int on){return Svc_Video_Ctrl(on?video_on:video_off);}
EXPORT void check_reinit(void){Svc_Video_Deinit();Svc_Video_Init();}
EXPORT void check_sensor_init(void){App_Sensor_Sync_Init();}
EXPORT int check_sensor_receive(Sensor_data_t* p){return App_Sensor_Sync_Receive(p);}
EXPORT void check_sensor_send(Sensor_data_t* p){App_Sensor_Sync_Send(p);}
EXPORT size_t check_encoding_capacity(void){return MAX_ENCODER_LEN;}
EXPORT void check_encode(uint8_t* input,size_t size,uint8_t* output){svc_frame_packet_t p={input,size};svc_encode_base64_with_jsonfilled(&p,output);}
EXPORT int check_app_capture_iterations(int n){check_loop_budget=n;current_video_state=video_start;App_Video_Streaming(0);return capture_calls;}
'''

source=prelude+'\n'+'\n'.join(defines)+'\n'+video+'\n'+sensor+'\n'+app+'\n'+extract_function(MBED_SOURCE.read_text(),'mbedtls_base64_encode')+'\n'+encoder+'\n'+wrappers
(BUILD/'host_harness.c').write_text(source,encoding='utf-8')
commands=[
    [str(CL),'/nologo','/utf-8','/c','/Od','/Oi-','/GS-','/Zl','/Fo'+str(BUILD/'host_harness.obj'),str(BUILD/'host_harness.c')],
    [str(LINK),'/nologo','/dll','/noentry','/nodefaultlib','/out:'+str(BUILD/'host_harness.dll'),str(BUILD/'host_harness.obj')],
]
logs=[]
for command in commands:
    result=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',errors='replace')
    logs.append({'command':command,'exit_code':result.returncode,'stdout':result.stdout,'stderr':result.stderr})
    (OUT/'host-compile.json').write_text(json.dumps(logs,ensure_ascii=False,indent=2),encoding='utf-8')
    if result.returncode:raise RuntimeError(result.stdout+result.stderr)

dll=C.CDLL(str(BUILD/'host_harness.dll'))
class Sensor(C.Structure):
    _fields_=[('temperature',C.c_float),('humidity',C.c_float),('light',C.c_uint16)]
dll.check_sensor_receive.argtypes=[C.POINTER(Sensor)]
dll.check_sensor_send.argtypes=[C.POINTER(Sensor)]
dll.check_encoding_capacity.restype=C.c_size_t
dll.check_held_length.restype=C.c_size_t
dll.check_encode.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p]
results=[]
def record(name,passed,evidence):results.append({'name':name,'passed':bool(passed),'evidence':evidence})
def fresh_video():dll.check_reset();dll.check_video_init(0)

dll.check_reset();dll.check_sensor_init();sample=Sensor(123,456,789)
ret=dll.check_sensor_receive(C.byref(sample))
record('sensor_empty_queue_returns_failure',ret!=0 and sample.temperature==123,{'return':ret})
record('sensor_null_output_returns_failure',dll.check_sensor_receive(None)!=0,{})
sample=Sensor(25.5,60.5,1000);dll.check_sensor_send(C.byref(sample));received=Sensor()
ret=dll.check_sensor_receive(C.byref(received))
record('sensor_valid_queue_roundtrip',ret==0 and received.temperature==25.5 and received.humidity==60.5 and received.light==1000,{'return':ret})

cloud=read('Components/App/App_Cloud_Upload/App_Cloud_Upload.c')
literal=re.search(r'sprintf\(sensor_data_str,\s*("(?:\\.|[^"\\])*")',cloud).group(1)
template=json.loads(literal)
sensor_capacity=int(re.search(r'^#define\s+APP_CLOUDE_SENSOR_DATA_UPLOAD_MAX_LEN\s+(\d+)',read('Components/App/App_Cloud_Upload/App_Cloud_Upload.h'),re.M).group(1))
crt=C.CDLL('msvcrt.dll');crt.sprintf.restype=C.c_int;buf=C.create_string_buffer(512)
ret=crt.sprintf(buf,template.encode(),C.c_double(25.5),C.c_double(60.5),C.c_int(1000))
parsed=json.loads(buf.value)
record('sensor_json_normal_values_parse',ret<sensor_capacity and parsed['reported']['temperature']==25.5,{'characters':ret,'capacity':sensor_capacity})
ret=crt.sprintf(buf,template.encode(),C.c_double(C.c_float(3.4e38).value),C.c_double(C.c_float(3.4e38).value),C.c_int(65535))
record('sensor_json_previous_extreme_finite_case_fits',ret+1<=sensor_capacity,{'characters':ret,'including_terminator':ret+1,'capacity':sensor_capacity})

fresh_video()
record('video_pool_initialization',dll.check_free()==6 and dll.check_queued()==0,{'free':dll.check_free(),'ready':dll.check_queued()})
dll.check_reset();dll.check_video_init(-1)
record('video_init_failure_does_not_mark_initialized',dll.check_initialized()==0 and dll.check_free()==-1,{'initialized':dll.check_initialized(),'free':dll.check_free()})
fresh_video();ret=dll.check_ready(0);hold=dll.check_hold();release=dll.check_release()
record('video_successful_frame_roundtrip',ret==0 and hold==0 and release==0 and dll.check_free()==6,{'free':dll.check_free()})
dll.check_reinit()
record('video_pool_reinit_without_active_consumer',dll.check_free()==6 and dll.check_queued()==0,{'free':dll.check_free(),'ready':dll.check_queued()})

fresh_video();ret=dll.check_ready(1)
record('video_capture_failure_propagates',ret!=0,{'return':ret})
record('video_failed_capture_preserves_buffer_count',dll.check_free()+dll.check_queued()==6,{'free':dll.check_free(),'ready':dll.check_queued()})
record('video_failed_capture_not_published_to_ready_queue',dll.check_queued()==0,{'ready':dll.check_queued(),'expected_ready':0})
hold=dll.check_hold()
record('video_consumer_cannot_hold_failed_frame',hold!=0,{'hold_return':hold,'frame_length':dll.check_held_length()})
dll.check_release()
fresh_video()
for _ in range(6):dll.check_ready(1)
ret=dll.check_ready(0)
record('video_capacity_survives_six_capture_failures',ret==0,{'next_capture_return':ret,'free':dll.check_free(),'ready':dll.check_queued()})

fresh_video();dll.check_ctrl(1)
captures=dll.check_app_capture_iterations(3)
record('video_app_continues_capture_across_three_iterations',captures==3,{'expected_capture_calls':3,'actual_capture_calls':captures,'limitation':'task loop bounded to three iterations; no scheduler'})

fresh_video();dll.check_ctrl(1);dll.check_ready(0);dll.check_hold()
original_byte=dll.check_held_byte();off_ret=dll.check_ctrl(0)
record('video_stop_preserves_consumer_ownership',dll.check_is_holding()==1 and dll.check_free()==5,{'off_return':off_ret,'holding_flag':dll.check_is_holding(),'free':dll.check_free(),'consumer_pointer_still_nonempty':dll.check_held_byte()!=-1})
dll.check_ctrl(1)
for _ in range(6):dll.check_ready(0)
record('video_held_buffer_not_reused_before_owner_release',dll.check_held_byte()==original_byte,{'original_first_byte':original_byte,'first_byte_after_interleaved_restart':dll.check_held_byte(),'limitation':'controlled sequential interleaving, not actual concurrent execution'})

capacity=dll.check_encoding_capacity()
for size in (3,30000,30717,30718,30719,30720):
    data=bytes(i%251 for i in range(size));inp=(C.c_ubyte*size).from_buffer_copy(data)
    out=(C.c_ubyte*(capacity+32))(*([0xA5]*(capacity+32)))
    dll.check_encode(inp,size,out)
    guard=bytes(out)[capacity:];changed=sum(x!=0xA5 for x in guard)
    try:
        payload=json.loads(bytes(out).split(b'\0',1)[0]);decoded=base64.b64decode(payload['params']['image'],validate=True);valid=decoded==data
    except (ValueError,KeyError,UnicodeDecodeError):valid=False
    record('base64_json_roundtrip_'+str(size),changed==0 and valid,{'frame_bytes':size,'allocation':capacity,'changed_guard_bytes':changed,'valid_roundtrip':valid})

files=['Components/App/App_Cloud_Upload/App_Cloud_Upload.c','Components/App/App_Cloud_Upload/App_Cloud_Upload.h','Components/App/App_Sensor_Sync/App_Sensor_Sync.c','Components/App/App_Video_Streaming/App_Video_Streaming.c','Components/Svc/Svc_Video/Svc_Video.c','Components/Svc/Svc_Video/Svc_Video.h','Components/Svc/Svc_Encode_Base64/Svc_Encode_Base64.c','Components/Svc/Svc_Encode_Base64/Svc_Encode_Base64.h','Components/Bsp/VIDEO/Video.c']
summary={'scope':'Host compilation of selected current C function bodies with RTOS, camera and allocator mocks. App task loop bounded to finite iterations. Base64 encoder body from project SDK ESP-IDF 6.0.1. Not target build, real FreeRTOS, real concurrency, hardware or cloud validation.','base64_encoder_source':str(MBED_SOURCE),'source_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in files},'checks':results,'passed':sum(r['passed'] for r in results),'failed':sum(not r['passed'] for r in results)}
(OUT/'results.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(summary,ensure_ascii=False,indent=2))
