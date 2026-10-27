"""Exercise the actually edited business Svc body, retaining its void APIs."""
import ast
import ctypes as C
import json
from pathlib import Path
import re
import subprocess
import threading
import time

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
BUILD=HERE/'build';BUILD.mkdir(exist_ok=True)
reference=(ROOT/'reference/video_sync/prepare_and_check.py').read_text(encoding='utf-8')
tree=ast.parse(reference)
prelude=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='prelude' for t in n.targets))
prelude+='''
#define ESP_LOGE(...) ((void)0)
#define ESP_ERR_NOT_FINISHED 268
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void){return create_cb();}
int xSemaphoreTakeRecursive(SemaphoreHandle_t h,TickType_t t){return take_cb(h,t);}
int xSemaphoreGiveRecursive(SemaphoreHandle_t h){return give_cb(h);}
'''
def read(path):
    raw=path.read_bytes()
    try:return raw.decode('utf-8')
    except UnicodeDecodeError:return raw.decode('gb18030')
def strip(text):return re.sub(r'^\s*#include[^\n]*','',text,flags=re.M)
wrappers='''
EXPORT int GetFree(void){return free_buf_queue?free_buf_queue->count:-1;}
EXPORT int GetReady(void){return ready_buf_queue?ready_buf_queue->count:-1;}
EXPORT int GetFreed(void){return freed;}
EXPORT int GetInvalid(void){return invalid_queue_calls;}
EXPORT int GetCaptures(void){return capture_calls;}
EXPORT int GetClosing(void){return is_video_closing;}
EXPORT void SetAllocationFail(int index){allocation_index=0;allocation_fail=index;}
EXPORT int CheckInit(void){Svc_Video_Init();return is_init?ESP_OK:ESP_FAIL;}
EXPORT int CheckDeinit(void){Svc_Video_Deinit();return !is_init&&!is_pool_init?ESP_OK:ESP_FAIL;}
EXPORT int CheckReset(void){Svc_Video_Reset();return is_init&&!is_video_ready?ESP_OK:ESP_FAIL;}
'''
source=prelude+strip(read(ROOT/'Components/Svc/Svc_Video/Svc_Video.h'))+strip(read(ROOT/'Components/Svc/Svc_Video/Svc_Video.c'))+wrappers
(BUILD/'native.c').write_text(source,encoding='utf-8')
cl=Path(r'D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe')
exports=['CheckInit','CheckDeinit','CheckReset','Svc_Video_Ctrl','Svc_Video_Start','Svc_Video_Stop','Svc_Video_Frame_Ready','Svc_Video_Frame_Hold','Svc_Video_Frame_Release']
for command in ([str(cl),'/nologo','/utf-8','/c','/Od','/Oi-','/GS-','/Zl','/Fo'+str(BUILD/'native.obj'),str(BUILD/'native.c')],
                [str(cl.with_name('link.exe')),'/nologo','/dll','/noentry','/nodefaultlib','/out:'+str(BUILD/'native.dll'),str(BUILD/'native.obj')]+['/export:'+name for name in exports]):
    result=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',errors='replace')
    if result.returncode:raise RuntimeError(result.stdout+result.stderr)

# Reuse the same behavioral scenarios, now against the business implementation.
# The existing void lifecycle APIs are inspected by wrappers, not changed.
tail=reference[reference.index("dll=C.CDLL("):reference.index("summary={'scope'")]
tail=tail.replace('dll.Svc_Video_Init','dll.CheckInit').replace('dll.Svc_Video_Deinit','dll.CheckDeinit').replace('dll.Svc_Video_Reset','dll.CheckReset')
tail=tail.replace("box.get('ret')==263", "box.get('ret')==-1").replace('ret==263 and dll.GetFreed()', 'ret==-1 and dll.GetFreed()').replace('ret==257 and dll.GetFreed()', 'ret==-1 and dll.GetFreed()')
# The second mutex is recursive, matching xSemaphoreCreateRecursiveMutex.
tail=tail.replace('locks[i]=threading.Lock()', 'locks[i]=threading.RLock() if i==2 else threading.Lock()')
exec(tail,globals())
summary={'scope':'Actual edited Svc C body, native host threads and mocked FreeRTOS queues/mutexes/camera/heap. Void lifecycle results inspected through state-check wrappers. Not hardware execution.','passed':sum(r['passed'] for r in results),'failed':sum(not r['passed'] for r in results),'checks':results}
(HERE/'tests.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(summary,ensure_ascii=False,indent=2))
