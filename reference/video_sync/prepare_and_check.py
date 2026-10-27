import concurrent.futures
import ctypes as C
import json
from pathlib import Path
import re
import subprocess
import threading
import time

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
BUILD=HERE/'build'
BUILD.mkdir(exist_ok=True)

def read(path):return path.read_text(encoding='utf-8',errors='replace')
def function_span(text,name):
    m=re.search(r'(?m)^void\s+'+name+r'\(',text)
    pos=text.index('{',m.end());depth=1;end=pos+1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return m.start(),end

upload=r'''void App_Cloud_Video_Frame_Upload(void *arg)
{
    const char *TAG = "video_upload";
    svc_frame_packet_t *frame_buf = NULL;
    unsigned char *encoded_buf = heap_caps_malloc(MAX_ENCODER_LEN, MALLOC_CAP_SPIRAM);
    if (encoded_buf == NULL) {
        ESP_LOGE(TAG, "Cannot allocate encoder buffer");
        vTaskDelete(NULL);
        return;
    }
    for (;;) {
        esp_err_t ret = Svc_Video_Frame_Hold(&frame_buf, pdMS_TO_TICKS(100));
        if (ret != ESP_OK) {
            /* Closed or no new frame: retry later, never use an old pointer. */
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        /* These are the existing encoder/MQTT calls. Their separate bounds
         * and publish-error handling are not changed by the synchronization fix. */
        svc_encode_base64_with_jsonfilled(frame_buf, encoded_buf);
        int published = Svc_Mqtt_Publish_For_VideoStream(USE_VIDEO_TOPIC_PUBLISH,
                                                       (char *)encoded_buf);
        /* Even a failed publish must end our use of this frame. */
        ESP_ERROR_CHECK(Svc_Video_Frame_Release(&frame_buf));
        if (published < 0) ESP_LOGW(TAG, "Video publish failed");
        vTaskDelay(APP_CLOUD_VIDEO_FRAME_UPLOAD_TASK_DELAY);
    }
}'''
original=read(ROOT/'Components/App/App_Cloud_Upload/App_Cloud_Upload.c')
a,b=function_span(original,'App_Cloud_Video_Frame_Upload')
(HERE/'App_Cloud_Upload.c').write_text(original[:a]+upload+original[b:],encoding='utf-8')
app=read(ROOT/'Components/App/App_Video_Streaming/App_Video_Streaming.c')
app=app.replace('Svc_Video_Reset();\n\t\t\t\t\tcurrent_video_state = video_idle;',
'''if (Svc_Video_Reset() == ESP_OK) {
                        get_frame_retry_count = 0;
                        current_video_state = video_idle;
                    } else {
                        ESP_LOGE(TAG, "Video reset failed or timed out");
                    }''')
app=app.replace('Svc_Video_Deinit();','''if (Svc_Video_Deinit() != ESP_OK) {
                ESP_LOGE(TAG, "Video deinit failed; resources retained");
            }''')
(HERE/'App_Video_Streaming.c').write_text(app,encoding='utf-8')

entries=json.loads(read(ROOT/'build/compile_commands.json'))
old=json.loads(read(ROOT/'build/project_description.json'))['project_path']
api=C.windll.shell32.CommandLineToArgvW
api.argtypes=[C.c_wchar_p,C.POINTER(C.c_int)];api.restype=C.POINTER(C.c_wchar_p)
C.windll.kernel32.LocalFree.argtypes=[C.c_void_p]
def args(command):
    n=C.c_int();p=api(command,C.byref(n))
    try:return [p[i] for i in range(n.value)]
    finally:C.windll.kernel32.LocalFree(p)
logs=[]
for entry in entries:
    name=Path(entry['file'].replace('\\','/')).name
    if name not in ('Svc_Video.c','App_Cloud_Upload.c','App_Video_Streaming.c','app_main.c'):continue
    command=entry['command'].replace(old,ROOT.as_posix()).replace(old.replace('/','\\'),str(ROOT))
    argv=args(command)
    argv.insert(1,'-I'+str(HERE))
    argv[argv.index('-o')+1]=str(BUILD/(name+'.obj'))
    if name!='app_main.c':argv[argv.index('-c')+1]=str(HERE/name)
    proc=subprocess.run(argv,cwd=ROOT/'build',capture_output=True,text=True,encoding='utf-8',errors='replace')
    logs.append({'file':name,'exit_code':proc.returncode,'diagnostics':proc.stdout+proc.stderr})
(HERE/'cross-compile.json').write_text(json.dumps(logs,ensure_ascii=False,indent=2),encoding='utf-8')
if any(x['exit_code'] for x in logs):raise RuntimeError(json.dumps(logs,ensure_ascii=False))

# Compile the reference Svc body on Windows, using Python-backed mutexes,
# real threads and a simulated driver. Queue operations stay in the native C.
prelude=r'''
typedef unsigned long long size_t;
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef int bool;
typedef int esp_err_t;
typedef uint32_t TickType_t;
typedef void* TaskHandle_t;
typedef void* SemaphoreHandle_t;
#define true 1
#define false 0
#define NULL ((void*)0)
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 257
#define ESP_ERR_INVALID_ARG 258
#define ESP_ERR_INVALID_STATE 259
#define ESP_ERR_INVALID_SIZE 260
#define ESP_ERR_NOT_FOUND 261
#define ESP_ERR_TIMEOUT 263
#define pdTRUE 1
#define pdMS_TO_TICKS(x) ((TickType_t)(x))
#define portMAX_DELAY ((TickType_t)0xffffffffu)
#define MALLOC_CAP_SPIRAM 0
#define EXPORT __declspec(dllexport)
void* memcpy(void* d,const void* s,size_t n){uint8_t* a=d;const uint8_t* b=s;for(size_t i=0;i<n;i++)a[i]=b[i];return d;}
void* memset(void* d,int c,size_t n){uint8_t* a=d;for(size_t i=0;i<n;i++)a[i]=(uint8_t)c;return d;}
typedef void* (*CreateCB)(void);
typedef int (*TakeCB)(void*,uint32_t);
typedef int (*GiveCB)(void*);
typedef uint32_t (*TickCB)(void);
typedef void* (*TaskCB)(void);
typedef void (*DelayCB)(uint32_t);
typedef int (*CameraCB)(int);
static CreateCB create_cb;static TakeCB take_cb;static GiveCB give_cb;
static TickCB tick_cb;static TaskCB task_cb;static DelayCB delay_cb;static CameraCB camera_cb;
EXPORT void Setup(CreateCB a,TakeCB b,GiveCB c,TickCB d,TaskCB e,DelayCB f,CameraCB g){create_cb=a;take_cb=b;give_cb=c;tick_cb=d;task_cb=e;delay_cb=f;camera_cb=g;}
SemaphoreHandle_t xSemaphoreCreateMutex(void){return create_cb();}
int xSemaphoreTake(SemaphoreHandle_t h,TickType_t t){return take_cb(h,t);}
int xSemaphoreGive(SemaphoreHandle_t h){return give_cb(h);}
void vSemaphoreDelete(SemaphoreHandle_t h){}
TickType_t xTaskGetTickCount(void){return tick_cb();}
TaskHandle_t xTaskGetCurrentTaskHandle(void){return task_cb();}
void vTaskDelay(TickType_t t){delay_cb(t);}
typedef struct {int count,head,tail,capacity,item_size,deleted;uint8_t items[8][16];} Queue;
typedef Queue* QueueHandle_t;
static Queue queues[64];static int queue_index,invalid_queue_calls,allocation_index,allocation_fail=-1,freed,capture_calls;
static uint8_t memory[6][30720];
QueueHandle_t xQueueCreate(int n,int s){Queue* q=&queues[queue_index++];memset(q,0,sizeof(*q));q->capacity=n;q->item_size=s;return q;}
int xQueueSend(QueueHandle_t q,const void* p,TickType_t t){if(!q||q->deleted){invalid_queue_calls++;return 0;}if(q->count==q->capacity)return 0;memcpy(q->items[q->tail],p,q->item_size);q->tail=(q->tail+1)%q->capacity;q->count++;return 1;}
int xQueueReceive(QueueHandle_t q,void* p,TickType_t t){if(!q||q->deleted){invalid_queue_calls++;return 0;}if(!q->count)return 0;memcpy(p,q->items[q->head],q->item_size);q->head=(q->head+1)%q->capacity;q->count--;return 1;}
void vQueueDelete(QueueHandle_t q){q->deleted=1;}
void* heap_caps_malloc(size_t n,int f){int i=allocation_index++;if(i==allocation_fail)return NULL;return memory[i%6];}
void heap_caps_free(void* p){if(p)freed++;}
typedef struct {uint8_t* payload;size_t length;} bsp_frame_packet_t;
int bsp_video_init(void){return camera_cb(0);}int bsp_video_deinit(void){return camera_cb(1);}
int bsp_video_ready(void){return camera_cb(2);}int bsp_video_off(void){return camera_cb(3);}
int bsp_video_start(void){return camera_cb(4);}int bsp_video_stop(void){return camera_cb(5);}
int bsp_video_get_frame(bsp_frame_packet_t* p){capture_calls++;int r=camera_cb(6);if(r)return r;p->length=3;p->payload[0]=(uint8_t)capture_calls;p->payload[1]=2;p->payload[2]=3;return 0;}
'''
def strip(text):return re.sub(r'^\s*#include[^\n]*','',text,flags=re.M)
wrappers=r'''
EXPORT int GetFree(void){return free_queue?free_queue->count:-1;}
EXPORT int GetReady(void){return ready_queue?ready_queue->count:-1;}
EXPORT int GetFreed(void){return freed;}
EXPORT int GetInvalid(void){return invalid_queue_calls;}
EXPORT int GetCaptures(void){return capture_calls;}
EXPORT int GetClosing(void){return closing;}
EXPORT void SetAllocationFail(int index){allocation_index=0;allocation_fail=index;}
'''
(BUILD/'native.c').write_text(prelude+strip(read(HERE/'Svc_Video.h'))+strip(read(HERE/'Svc_Video.c'))+wrappers,encoding='utf-8')
cl=Path(r'D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe')
exports=['Svc_Video_Init','Svc_Video_Deinit','Svc_Video_Reset','Svc_Video_Ctrl','Svc_Video_Start','Svc_Video_Stop','Svc_Video_Frame_Ready','Svc_Video_Frame_Hold','Svc_Video_Frame_Release']
for command in ([str(cl),'/nologo','/utf-8','/c','/Od','/Oi-','/GS-','/Zl','/Fo'+str(BUILD/'native.obj'),str(BUILD/'native.c')],
                [str(cl.with_name('link.exe')),'/nologo','/dll','/noentry','/nodefaultlib','/out:'+str(BUILD/'native.dll'),str(BUILD/'native.obj')]+['/export:'+name for name in exports]):
    p=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',errors='replace')
    if p.returncode:raise RuntimeError(p.stdout+p.stderr)

dll=C.CDLL(str(BUILD/'native.dll'))
locks={};lock_ids=iter(range(1,100));camera_failure=False;capture_entered=threading.Event();capture_continue=threading.Event();block_capture=False
Create=C.CFUNCTYPE(C.c_void_p);Take=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint32);Give=C.CFUNCTYPE(C.c_int,C.c_void_p)
Tick=C.CFUNCTYPE(C.c_uint32);Task=C.CFUNCTYPE(C.c_void_p);Delay=C.CFUNCTYPE(None,C.c_uint32);Camera=C.CFUNCTYPE(C.c_int,C.c_int)
def create():i=next(lock_ids);locks[i]=threading.Lock();return i
def take(h,t):return int(locks[h].acquire() if t==0xffffffff else locks[h].acquire(timeout=t/1000))
def give(h):locks[h].release();return 1
def camera(op):
    if op==6:
        if block_capture:
            capture_entered.set();capture_continue.wait(5)
        return -1 if camera_failure else 0
    return 0
callbacks=[Create(create),Take(take),Give(give),Tick(lambda:int(time.monotonic()*1000)&0xffffffff),Task(threading.get_ident),Delay(lambda t:time.sleep(t/1000)),Camera(camera)]
dll.Setup.argtypes=[Create,Take,Give,Tick,Task,Delay,Camera];dll.Setup(*callbacks)
class Frame(C.Structure):_fields_=[('payload',C.POINTER(C.c_uint8)),('length',C.c_size_t)]
FP=C.POINTER(Frame)
dll.Svc_Video_Frame_Hold.argtypes=[C.POINTER(FP),C.c_uint32]
dll.Svc_Video_Frame_Release.argtypes=[C.POINTER(FP)]
results=[]
def record(name,ok,**evidence):results.append({'name':name,'passed':bool(ok),'evidence':evidence})
def worker(fn):
    box={}
    def call():box['ret']=fn()
    t=threading.Thread(target=call);t.start();return t,box
def fresh():
    assert dll.Svc_Video_Deinit()==0
    dll.SetAllocationFail(-1)
    assert dll.Svc_Video_Init()==0 and dll.Svc_Video_Ctrl(1)==0 and dll.Svc_Video_Start()==0

fresh();before=dll.GetCaptures()
for _ in range(3):assert dll.Svc_Video_Start()==0 and dll.Svc_Video_Frame_Ready()==0
record('repeated_start_keeps_capturing',dll.GetCaptures()-before==3)
fresh();camera_failure=True
for _ in range(6):assert dll.Svc_Video_Frame_Ready()!=0
record('failed_capture_returns_buffer_without_ready_frame',dll.GetFree()==6 and dll.GetReady()==0,free=dll.GetFree(),ready=dll.GetReady())
camera_failure=False
assert dll.Svc_Video_Frame_Ready()==0
p=FP();assert dll.Svc_Video_Frame_Hold(C.byref(p),0)==0
value=p.contents.payload[0];t,box=worker(lambda:dll.Svc_Video_Ctrl(0));time.sleep(.05)
record('close_does_not_force_release',t.is_alive() and dll.GetFree()==5 and p.contents.payload[0]==value)
assert dll.Svc_Video_Frame_Release(C.byref(p))==0;t.join(3)
record('owner_release_unblocks_close',not bool(p) and box.get('ret')==0)

fresh();assert dll.Svc_Video_Frame_Ready()==0;p=FP();assert dll.Svc_Video_Frame_Hold(C.byref(p),0)==0
free_before=dll.GetFreed();t,box=worker(dll.Svc_Video_Deinit);time.sleep(.05)
record('deinit_waits_before_freeing_payloads',t.is_alive() and dll.GetFreed()==free_before)
assert dll.Svc_Video_Frame_Release(C.byref(p))==0;t.join(3)
record('deinit_frees_after_release',box.get('ret')==0 and dll.GetFreed()==free_before+6)

fresh();assert dll.Svc_Video_Frame_Ready()==0;p=FP();assert dll.Svc_Video_Frame_Hold(C.byref(p),0)==0
free_before=dll.GetFreed();t,box=worker(dll.Svc_Video_Deinit);t.join(3)
record('close_timeout_preserves_borrowed_memory',box.get('ret')==263 and bool(p) and dll.GetFreed()==free_before)
assert dll.Svc_Video_Frame_Release(C.byref(p))==0
record('deinit_retry_after_late_release',dll.Svc_Video_Deinit()==0)

fresh()
def waiting_hold():
    local=FP();return dll.Svc_Video_Frame_Hold(C.byref(local),0xffffffff)
t,box=worker(waiting_hold);time.sleep(.03);ret=dll.Svc_Video_Deinit();t.join(1)
record('blocked_hold_exits_without_accessing_deleted_queue',ret==0 and box.get('ret')==259 and dll.GetInvalid()==0)

fresh();assert dll.Svc_Video_Frame_Ready()==0;p=FP();assert dll.Svc_Video_Frame_Hold(C.byref(p),0)==0
t,box=worker(lambda:dll.Svc_Video_Frame_Release(C.byref(p)));t.join(1)
record('different_task_cannot_release_owner_frame',box.get('ret')==259 and bool(p))
assert dll.Svc_Video_Frame_Release(C.byref(p))==0

fresh();assert dll.Svc_Video_Frame_Ready()==0;p=FP();assert dll.Svc_Video_Frame_Hold(C.byref(p),0)==0
free_before=dll.GetFreed();t,box=worker(dll.Svc_Video_Reset);time.sleep(.03)
assert dll.Svc_Video_Frame_Release(C.byref(p))==0;t.join(3)
record('reset_waits_and_keeps_pool',box.get('ret')==0 and dll.GetFreed()==free_before and dll.GetFree()==6)

fresh();block_capture=True;capture_entered.clear();capture_continue.clear()
t,box=worker(dll.Svc_Video_Frame_Ready);assert capture_entered.wait(1)
free_before=dll.GetFreed();ret=dll.Svc_Video_Deinit()
record('blocked_camera_prevents_unsafe_deinit',ret==263 and dll.GetFreed()==free_before)
block_capture=False;capture_continue.set();t.join(2)
assert dll.Svc_Video_Deinit()==0

dll.SetAllocationFail(2);free_before=dll.GetFreed();ret=dll.Svc_Video_Init()
record('partial_pool_allocation_rolls_back',ret==257 and dll.GetFreed()==free_before+2)
dll.SetAllocationFail(-1)
record('init_recovers_after_allocation_failure',dll.Svc_Video_Init()==0)
assert dll.Svc_Video_Deinit()==0
summary={'scope':'Reference Svc C body with real host threads/Python-backed mutexes, fake camera/queues/heap. Not ESP FreeRTOS/hardware execution. Original business files are unchanged.','passed':sum(r['passed'] for r in results),'failed':sum(not r['passed'] for r in results),'checks':results}
(HERE/'tests.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'target_compile':logs,'tests':summary},ensure_ascii=False,indent=2))
