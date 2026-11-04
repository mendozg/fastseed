from pathlib import Path
import ast,sys,subprocess,os
root=Path('work/fastseed-v2-alpha3-servicefilter-r1').resolve()
out=Path('work/alpha3-servicefilter-test-results').resolve()
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

import re
count=0
for path in sorted((root/'docs').glob('*.md')):
 if path.name != 'DNS-SERVICE-FILTER.md':continue
 for block in re.findall(r'```bash\n(.*?)```',path.read_text(),re.S):
  assert not parser.parse(block.encode()).root_node.has_error,path
  count+=1
with (out/'static-validation.log').open('a',encoding='utf-8') as log:
 log.write(f'PASS: {count} new Bash documentation examples parsed (not executed)\n')
print(f'PASS: {count} new Bash documentation examples parsed (not executed)')
