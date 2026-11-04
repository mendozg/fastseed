from pathlib import Path
import concurrent.futures
import os
import re
import shutil
import subprocess
import sys
root=Path('work/fastseed-v2-alpha3/source').resolve()
toolchain=Path('work/toolchains/llvm-mingw-20261020-ucrt-x86_64/bin').resolve()
deps=Path('work/alpha2-dependencies/ucrt64').resolve()
out=Path('work/alpha3-test-results').resolve()
env=os.environ.copy();env['PATH']=str(toolchain)+';'+str(deps/'bin')+';'+env['PATH']
cmake=(root/'CMakeLists.txt').read_text()
sources=re.findall(r'src/[\w/]+\.cpp',cmake.split('add_library(fastseed',1)[1].split(')',1)[0])
sources=[s for s in sources if '/db/' not in s]
sources.append('src/canary/mainnet_canary.cpp')
tests=re.findall(r'add_executable\((\w+_tests) (tests/[^ )]+\.cpp)\)',cmake)
tests=[(n,s) for n,s in tests if '/db/' not in s]
tests += [(f'protocol_{n}_tests',f'tests/protocol/test_{n}_v2.cpp') for n in ['serializer','codec','wire']]
tests += [(f'peer_{n}_tests',f'tests/peer/test_{n}.cpp') for n in ['identity','cache','selection']]
profile=sys.argv[1] if len(sys.argv)>1 else 'Debug'
flags={'Debug':['-O0','-g'],'Release':['-O2','-DNDEBUG','-UNDEBUG'],'ASan':['-O1','-g','-fsanitize=address','-fno-omit-frame-pointer']}[profile]
build=out/profile;build.mkdir(parents=True,exist_ok=True)
temp=build/'tmp';temp.mkdir(exist_ok=True);env['TEMP']=env['TMP']=str(temp)
shutil.copytree(root/'config',build/'config',dirs_exist_ok=True)
common=[toolchain/'clang++.exe','-std=c++20','-Wall','-Wextra','-Wpedantic','-DBOOST_ERROR_CODE_HEADER_ONLY','-DBOOST_SYSTEM_NO_DEPRECATED','-I',root/'include','-isystem',deps/'include',*flags]
def run(args,timeout=180):
    p=subprocess.run(list(map(str,args)),capture_output=True,text=True,env=env,cwd=build,timeout=timeout)
    return 'COMMAND: '+' '.join(map(str,args))+'\n'+p.stdout+p.stderr,p.returncode
with (build/'run.log').open('w',encoding='utf-8',newline='\n') as log:
    log.write('Native Windows component regression. Database/POSIX lock tests excluded. Not a Linux release gate.\n')
    text,code=run([toolchain/'clang++.exe','--version']);log.write(text)
    def compile(source):
        obj=build/(source.replace('/','_')+'.o')
        latest=max([p.stat().st_mtime for p in (root/'include').rglob('*') if p.is_file()]+[(root/source).stat().st_mtime])
        if obj.exists() and obj.stat().st_mtime>=latest:return obj,'UNCHANGED OBJECT: '+str(obj)+'\n',0
        strict=['-Werror'] if any(x in source for x in ['/peer/','/health/','/candidate.cpp']) else []
        result,code=run([*common,*strict,'-c',root/source,'-o',obj]);return obj,result,code
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        compiled=list(pool.map(compile,sources))
    errors=[]
    for obj,result,code in compiled:
        log.write(result)
        if code:print(result,flush=True);errors.append(obj.name)
    if errors:raise SystemExit(f'Compile failed: {errors}')
    library=build/'components.a';result,code=run([toolchain/'llvm-ar.exe','rcs',library,*[obj for obj,_,_ in compiled]]);log.write(result)
    if code:raise SystemExit('archive failed')
    print(f'{profile}: compiled {len(sources)} component sources',flush=True)
    selected=set(sys.argv[2:])
    if selected:tests=[(n,s) for n,s in tests if n in selected]
    passed=[];failed=[]
    for name,source in tests:
        exe=build/(name+'.exe');strict=['-Werror'] if '/peer/' in source else []
        result,code=run([*common,*strict,root/source,library,deps/'lib/libcrypto.dll.a','-lws2_32','-lmswsock','-o',exe]);log.write(result);log.flush()
        if code:
            failed.append(name);print(f'BUILD FAILED: {name}\n{result[-3500:]}',flush=True);continue
        result,code=run([exe],90);log.write(result);log.flush()
        if code:failed.append(name);print(f'FAIL: {name}\n{result[-3000:]}',flush=True)
        else:passed.append(name);print(f'{profile} PASS: {name} '+result.split('\n',1)[1].strip(),flush=True)
    summary=f'{profile}: {len(passed)}/{len(tests)} selected suites passed. Failed: {failed}\n'
    print(summary,flush=True);log.write(summary)
    (build/'summary.txt').write_text(summary+'\n'.join(passed)+'\n',encoding='utf-8')
    if failed:raise SystemExit(1)
