#!/usr/bin/env python3
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SDK = os.path.join(ROOT, '3rd-party', 'physx')
PHYSX = os.path.join(SDK, 'physx')
ARCH = sys.argv[1] if len(sys.argv) > 1 else 'x64'
SUFFIX = '64' if ARCH == 'x64' else '32'
OUT = os.path.join(SDK, 'lib')
OBJ = os.path.join(SDK, 'obj')

manifest = json.load(open(os.path.join(ROOT, 'scripts', 'physx_manifest.json')))
sources = manifest['sources'] + manifest['sources_windows']
includes = [os.path.normpath(os.path.join(PHYSX, inc)) for inc in manifest['includes']]

DEFINES = manifest['defines'] + [
    '_CRT_SECURE_NO_WARNINGS',
    '_WINSOCK_DEPRECATED_NO_WARNINGS',
    '_ITERATOR_DEBUG_LEVEL=0',
]
FLAGS = ['/nologo', '/c', '/std:c++14', '/O2', '/MD', '/MP', '/w', '/GF', '/GS-', '/GR-', '/Gd', '/EHsc']

os.makedirs(OUT, exist_ok=True)

groups = {}
for src in sources:
    groups.setdefault(os.path.dirname(src), []).append(src)

objs = []
for i, (group, files) in enumerate(sorted(groups.items())):
    objdir = os.path.join(OBJ, '%02d' % i)
    os.makedirs(objdir, exist_ok=True)
    cmd = ['cl'] + FLAGS + ['/D' + d for d in DEFINES] + \
          ['/I' + inc for inc in includes] + \
          ['/Fo%s\\' % objdir] + \
          [os.path.normpath(os.path.join(PHYSX, f)) for f in files]
    r = subprocess.run(cmd)
    if r.returncode != 0:
        print('FAILED compiling %s' % group)
        sys.exit(1)
    objs += [os.path.join(objdir, f) for f in os.listdir(objdir) if f.endswith('.obj')]

lib = os.path.join(OUT, 'physx_static_%s.lib' % SUFFIX)
rsp = os.path.join(OBJ, 'lib.rsp')
with open(rsp, 'w') as f:
    f.write('\n'.join('"%s"' % o for o in objs))
r = subprocess.run(['lib', '/nologo', '/OUT:' + lib, '@' + rsp])
if r.returncode != 0:
    sys.exit(1)
print('OK %s (%d objects)' % (lib, len(objs)))
