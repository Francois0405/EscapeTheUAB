"""Flag quoted or angled includes of project files whose letter case differs from the tracked path."""
import os
import posixpath
import re
import subprocess
import sys

os.chdir(sys.argv[1])
rev = sys.argv[2]
tracked = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', rev]).decode('utf-8', 'replace').split('\n')
exact = set(tracked)
lower = {p.lower(): p for p in tracked}
if any(p.startswith('src/CMakeLists') for p in tracked):
    code_dirs = ('src/', 'tests/')
    extra = ['', 'src/legacy_vgi', 'src/scene', 'src/platform', 'third_party', 'third_party/imgui', 'third_party/stb_compat',
             'third_party/imguizmo', 'third_party/SOIL2/src', 'third_party/cgltf', 'third_party/miniaudio']
else:
    code_dirs = ('src/', 'tests/', 'EntornVGI/')
    extra = ['', 'EntornVGI', 'src/legacy_vgi', 'src/scene', 'src/platform', 'third_party', 'third_party/imgui',
             'EntornVGI/third_party', 'third_party/imguizmo', 'third_party/SOIL2/src', 'third_party/cgltf', 'third_party/miniaudio']
bad = 0
for path in tracked:
    if not path.startswith(code_dirs) or not path.endswith(('.cpp', '.h', '.c')):
        continue
    text = subprocess.check_output(['git', 'show', f'{rev}:{path}']).decode('latin-1')
    for inc in re.findall(r'(?m)^[ \t]*#[ \t]*include[ \t]*["<]([^">]+)[">]', text):
        candidates = [posixpath.normpath(posixpath.join(d, inc)) for d in [posixpath.dirname(path)] + extra]
        if any(c in exact for c in candidates):
            continue
        near = [lower[c.lower()] for c in candidates if c.lower() in lower]
        if near:
            bad += 1
            print(f'{path}: "{inc}" -> {near[0]}')
print(f'{rev}: {bad} case mismatches')
