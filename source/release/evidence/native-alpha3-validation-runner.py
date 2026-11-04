from pathlib import Path
import ast,sys,subprocess,os
root=Path('work/fastseed-v2-alpha3').resolve()
out=Path('work/alpha3-test-results').resolve()
sys.path.insert(0,str(Path('work/bash-validation-ts').resolve()))
from tree_sitter import Language,Parser
import tree_sitter_bash
parser=Parser(Language(tree_sitter_bash.language()))
lines=[]
for p in sorted(root.rglob('*.sh')):
 if '.git' in p.parts:continue
 assert not parser.parse(p.read_bytes()).root_node.has_error,p
 lines.append('PASS Bash grammar: '+p.relative_to(root).as_posix())
for p in sorted(root.rglob('*.py')):
 if '.git' in p.parts:continue
 ast.parse(p.read_text(),filename=str(p));lines.append('PASS Python AST: '+p.relative_to(root).as_posix())
for rel,args in [('source/scripts/verify-version.py',[]),('source/tests/protocol/generate_vectors.py',['--check']),('source/tests/peer/generate_vectors.py',['--check'])]:
 p=subprocess.run([sys.executable,str(root/rel),*args],capture_output=True,text=True)
 lines.append(p.stdout+p.stderr);assert p.returncode==0
actionlint=Path('work/actionlint/actionlint.exe').resolve()
p=subprocess.run([str(actionlint),'-color=false','-shellcheck=','-pyflakes=','.github/workflows/ci.yml'],cwd=root,capture_output=True,text=True)
lines.append('Actionlint 1.7.12: '+('PASS' if p.returncode==0 else p.stdout+p.stderr));assert p.returncode==0
(out/'static-validation.log').write_text('\n'.join(lines)+'\n',encoding='utf-8',newline='\n')
print(f'PASS: static shell/Python, version/fixture checks and actionlint ({len(lines)} checks)')
toolchain=Path('work/toolchains/llvm-mingw-20261020-ucrt-x86_64/bin').resolve()
deps=Path('work/alpha2-dependencies/ucrt64').resolve()
env=os.environ.copy();env['PATH']=str(toolchain)+';'+str(deps/'bin')+';'+env['PATH']
lines=['COMPILE ONLY: four database test translation units; not linked or executed. POSIX database locking unavailable in native build.']
for source in sorted((root/'source/tests/db').glob('*.cpp')):
 args=[str(toolchain/'clang++.exe'),'-std=c++20','-O0','-Wall','-Wextra','-Wpedantic','-DBOOST_ERROR_CODE_HEADER_ONLY','-DBOOST_SYSTEM_NO_DEPRECATED','-I',str(root/'source/include'),'-isystem',str(deps/'include'),'-c',str(source),'-o',str(out/(source.stem+'.o'))]
 p=subprocess.run(args,capture_output=True,text=True,env=env)
 lines.append('COMMAND: '+' '.join(args)+'\n'+p.stdout+p.stderr);assert p.returncode==0,(source,p.stderr)
 lines.append('COMPILE PASS: '+source.name)
(out/'database-compile-only.log').write_text('\n'.join(lines)+'\n',encoding='utf-8',newline='\n')
print('PASS: four DB test translation units compiled (NOT EXECUTED)')
