"""Recompile current owned C files with cached target compiler arguments.
Paths/output destinations are relocated; flags and SDK config remain unchanged.
This validates translation units, not a fresh CMake configure or firmware link.
"""
import concurrent.futures
import ctypes as C
import json
from pathlib import Path
import subprocess

OUT=Path(__file__).resolve().parent
ROOT=OUT.parents[1]
DEST=OUT/'build'/'target_objects'
DEST.mkdir(exist_ok=True)
entries=json.loads((ROOT/'build'/'compile_commands.json').read_text())
old=json.loads((ROOT/'build'/'project_description.json').read_text())['project_path']
api=C.windll.shell32.CommandLineToArgvW
api.argtypes=[C.c_wchar_p,C.POINTER(C.c_int)]
api.restype=C.POINTER(C.c_wchar_p)
C.windll.kernel32.LocalFree.argtypes=[C.c_void_p]

def arguments(command):
    n=C.c_int();p=api(command,C.byref(n))
    try:return [p[i] for i in range(n.value)]
    finally:C.windll.kernel32.LocalFree(p)

def compile_one(entry):
    command=entry['command'].replace(old,ROOT.as_posix()).replace(old.replace('/','\\'),str(ROOT))
    args=arguments(command)
    name=Path(entry['file'].replace('\\','/')).stem
    for i,arg in enumerate(args):
        if arg=='-o':args[i+1]=str(DEST/(name+'.obj'))
        elif arg=='-MF':args[i+1]=str(DEST/(name+'.d'))
        elif arg=='-MT':args[i+1]=str(DEST/(name+'.obj'))
    proc=subprocess.run(args,cwd=ROOT/'build',capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=120)
    return {'file':entry['file'].replace(old.replace('/','\\'),str(ROOT)).replace(old,ROOT.as_posix()),'exit_code':proc.returncode,'command':args,'stdout':proc.stdout,'stderr':proc.stderr}

owned=[]
for entry in entries:
    path=entry['file'].replace('\\','/')
    if any('/'+part+'/' in path for part in ('Components/App','Components/Svc','Components/Bsp/VIDEO','Components/Bsp/KEY','Components/Bsp/XL9555','Components/Bsp/LED','Components/Bsp/WIFI_CONNECT')) or path.endswith('/main/app_main.c'):
        owned.append(entry)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:results=list(pool.map(compile_one,owned))
summary={'scope':'Actual Xtensa cross-compilation of owned C translation units using cached generated config/includes/flags, with project paths and object destinations relocated. No CMake reconfiguration, full dependency rebuild, firmware link, flash, or hardware run.','passed':sum(r['exit_code']==0 for r in results),'failed':sum(r['exit_code']!=0 for r in results),'files':results}
(OUT/'cross-compile.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'passed':summary['passed'],'failed':summary['failed'],'results':[{'file':r['file'],'exit_code':r['exit_code'],'diagnostics':r['stderr'][:2500]} for r in results]},ensure_ascii=False,indent=2))
