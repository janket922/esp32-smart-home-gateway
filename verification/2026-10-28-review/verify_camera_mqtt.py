"""Run current camera boundary paths and MQTT callbacks with native C mocks."""
import ctypes as C
import json
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
BUILD = HERE / 'build'
BUILD.mkdir(exist_ok=True)

def read(path):
    raw = path.read_bytes()
    try:
        return raw.decode('utf-8')
    except UnicodeDecodeError:
        return raw.decode('gb18030')

prelude = r'''
typedef unsigned long long size_t;
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef int esp_err_t;
#define NULL ((void*)0)
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 258
#define ESP_ERR_INVALID_STATE 259
#define ESP_VIDEO_DVP_DEVICE_NAME "/dev/video2"
#define BSP_VIDEO_MAX_FRAME_SIZE (30*1024)
#define MAP_FAILED ((void*)-1)
#define O_RDWR 2
#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_SHARED 1
#define V4L2_BUF_TYPE_VIDEO_CAPTURE 1
#define V4L2_PIX_FMT_JPEG 1
#define V4L2_FIELD_NONE 0
#define V4L2_COLORSPACE_DEFAULT 0
#define V4L2_MEMORY_MMAP 1
#define VIDIOC_REQBUFS 1
#define VIDIOC_S_FMT 2
#define VIDIOC_QUERYBUF 3
#define VIDIOC_QBUF 4
#define VIDIOC_DQBUF 5
#define VIDIOC_STREAMON 6
#define VIDIOC_STREAMOFF 7
typedef struct {uint8_t* payload;size_t length;} bsp_frame_packet_t;
struct v4l2_requestbuffers {uint32_t count;int type,memory;};
struct v4l2_buffer {uint32_t index;int type,memory;uint32_t length,bytesused;union {int offset;} m;};
struct v4l2_format {int type;struct {struct {int width,height,pixelformat,field,sizeimage,colorspace;} pix;} fmt;};
static unsigned char memory[4][128];
static unsigned int requested=4,frame_index,frame_size=4;
static int map_fail=-1,maps,closes,unmaps,queued,fail_requeue;
static size_t unmapped_bytes;
static const char* TAG="test";
static void log_sink(const char* tag,const char* fmt,...){(void)tag;(void)fmt;}
#define ESP_LOGE(...) log_sink(__VA_ARGS__)
#define ESP_LOGI(...) log_sink(__VA_ARGS__)
#define ESP_LOGW(...) log_sink(__VA_ARGS__)
#define ESP_LOGD(...) log_sink(__VA_ARGS__)
void* memcpy(void* d,const void* s,size_t n){unsigned char* a=d;const unsigned char* b=s;for(size_t i=0;i<n;i++)a[i]=b[i];return d;}
int open(const char* name,int mode){(void)name;(void)mode;return 7;}
int close(int fd){(void)fd;closes++;return 0;}
void* mmap(void* a,size_t n,int p,int flags,int fd,int offset){(void)a;(void)n;(void)p;(void)flags;(void)fd;if(maps++==map_fail)return NULL;return memory[offset];}
int munmap(void* a,size_t n){(void)a;unmaps++;unmapped_bytes+=n;return 0;}
int ioctl(int fd,int req,void* p){
    (void)fd;
    if(req==VIDIOC_REQBUFS){struct v4l2_requestbuffers* b=p;if(b->count)b->count=requested;return 0;}
    if(req==VIDIOC_QUERYBUF){struct v4l2_buffer* b=p;b->length=64;b->m.offset=b->index;return 0;}
    if(req==VIDIOC_DQBUF){struct v4l2_buffer* b=p;b->index=frame_index;b->bytesused=frame_size;return 0;}
    if(req==VIDIOC_QBUF){queued++;return fail_requeue?-1:0;}
    return 0;
}
'''
camera = read(ROOT / 'Components/Bsp/VIDEO/Video.c')
source = prelude + camera[camera.index('#define BSP_MMAP_BUFFER_COUNT'):]
source += r'''
typedef const char* esp_event_base_t;
typedef void* esp_mqtt_client_handle_t;
typedef int esp_mqtt_event_id_t;
typedef struct {int error_type,esp_tls_last_esp_err,esp_tls_stack_err,esp_transport_sock_errno,connect_return_code;} Error;
typedef struct {void* client;int msg_id;char* data;Error* error_handle;int event_id;} Event;
typedef Event* esp_mqtt_event_handle_t;
#define PRIi32 "d"
enum {MQTT_EVENT_CONNECTED=1,MQTT_EVENT_DISCONNECTED,MQTT_EVENT_SUBSCRIBED,MQTT_EVENT_UNSUBSCRIBED,MQTT_EVENT_PUBLISHED,MQTT_EVENT_DATA,MQTT_EVENT_ERROR};
#define MQTT_ERROR_TYPE_TCP_TRANSPORT 1
#define MQTT_ERROR_TYPE_CONNECTION_REFUSED 2
#define USE_TOPIC_SUBSCERIBE "test"
static int subscribed;
int esp_mqtt_client_subscribe(void* c,const char* t,int q){(void)c;(void)t;(void)q;subscribed++;return 1;}
const char* strerror(int e){(void)e;return "error";}
'''
mqtt = read(ROOT / 'Components/Svc/Svc_Mqtt/Svc_Mqtt.c')
source += mqtt[mqtt.index('static void mqtt_event_handler'):mqtt.index('void Svc_Mqtt_Start')]
source += r'''
#define EXPORT __declspec(dllexport)
EXPORT void setup(unsigned int count,int failure){
    bsp_video_off();requested=count;frame_index=0;frame_size=4;map_fail=failure;
    maps=closes=unmaps=queued=fail_requeue=0;unmapped_bytes=0;
    for(int i=0;i<4;i++)for(int j=0;j<128;j++)memory[i][j]=(unsigned char)(j+1);
}
EXPORT int ready(void){return bsp_video_ready();}
EXPORT int capture(unsigned int index,unsigned int bytes,int queue_failure){
    static unsigned char output[30752];for(int i=0;i<30752;i++)output[i]=0xA5;
    frame_index=index;frame_size=bytes;fail_requeue=queue_failure;
    bsp_frame_packet_t p={output,99};int ret=bsp_video_get_frame(&p);
    for(int i=30720;i<30752;i++)if(output[i]!=0xA5)return -99;
    if(ret!=0 && p.length!=0)return -98;
    if(ret==0 && (p.length!=bytes || output[0]!=1))return -97;
    return ret;
}
EXPORT int off(void){return bsp_video_off();}
EXPORT int count_closes(void){return closes;}
EXPORT int count_unmaps(void){return unmaps;}
EXPORT size_t unmapped_length(void){return unmapped_bytes;}
EXPORT int mqtt_null_events(void){
    Event event={0};mqtt_event_handler(NULL,"MQTT",MQTT_EVENT_SUBSCRIBED,&event);
    mqtt_event_handler(NULL,"MQTT",MQTT_EVENT_ERROR,&event);
    mqtt_event_handler(NULL,"MQTT",MQTT_EVENT_CONNECTED,&event);
    mqtt_event_handler(NULL,"MQTT",MQTT_EVENT_DATA,NULL);
    return subscribed==1;
}
'''
(BUILD / 'camera_mqtt.c').write_text(source, encoding='utf-8')
cl = Path(r'D:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe')
for command in ([str(cl), '/nologo', '/utf-8', '/c', '/Od', '/Oi-', '/GS-', '/Zl',
                 '/Fo' + str(BUILD / 'camera_mqtt.obj'), str(BUILD / 'camera_mqtt.c')],
                [str(cl.with_name('link.exe')), '/nologo', '/dll', '/noentry', '/nodefaultlib',
                 '/out:' + str(BUILD / 'camera_mqtt.dll'), str(BUILD / 'camera_mqtt.obj')]):
    result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', errors='replace')
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
dll = C.CDLL(str(BUILD / 'camera_mqtt.dll'))
dll.unmapped_length.restype = C.c_size_t
results = []

def check(name, passed):
    results.append({'name': name, 'passed': bool(passed)})

dll.setup(4, -1)
check('normal setup', dll.ready() == 0)
check('valid frame copy', dll.capture(0, 4, 0) == 0)
check('driver index outside mappings', dll.capture(4, 4, 0) == -1)
check('frame exceeds actual mapping', dll.capture(0, 65, 0) == -1)
check('frame exceeds output capacity', dll.capture(0, 30721, 0) == -1)
check('empty frame rejected', dll.capture(0, 0, 0) == -1)
check('requeue failure invalidates frame', dll.capture(0, 4, 1) == -1)
check('unmap uses queried lengths', dll.off() == 0 and dll.count_unmaps() == 4 and dll.unmapped_length() == 256)
check('repeated cleanup', dll.off() == 0 and dll.count_closes() == 1)
check('capture on closed camera', dll.capture(0, 4, 0) != 0)
dll.setup(4, 1)
check('NULL mapping rolls back partial setup', dll.ready() == -1 and dll.count_closes() == 1 and dll.count_unmaps() == 1)
dll.setup(5, -1)
check('oversized mapping count rejected', dll.ready() == -1 and dll.count_closes() == 1)
dll.setup(0, -1)
check('empty mapping count rejected', dll.ready() == -1 and dll.count_closes() == 1)
check('MQTT null data/error/event handled', dll.mqtt_null_events() == 1)
wifi = read(ROOT / 'Components/Bsp/WIFI_CONNECT/WIFI_CONNECT.c')
check('Wi-Fi callbacks use static configuration', wifi.count('(void *)&s_wifi_config,') == 2
      and 'static wifi_sta_cfg_t s_wifi_config;' in wifi and 's_wifi_config = *config;' in wifi)
summary = {'scope': 'Actual camera C bodies and MQTT event handler with native driver/log mocks; Wi-Fi lifetime checked structurally; no hardware.',
           'passed': sum(r['passed'] for r in results), 'failed': sum(not r['passed'] for r in results), 'checks': results}
(HERE / 'camera-mqtt-tests.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
print(json.dumps(summary, indent=2))
raise SystemExit(bool(summary['failed']))
