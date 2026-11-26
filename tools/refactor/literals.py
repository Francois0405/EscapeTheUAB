"""Compare every string/char literal (raw strings included) between two revisions of the code."""
import collections
import re
import subprocess
import sys

ROOT, A, B = sys.argv[1:4]
TOKEN = re.compile(r'//[^\n]*|/\*.*?\*/|(?:u8|[uUL])?R"([^ ()\\\t\n]{0,16})\((.*?)\)\1"|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'',
                   re.S)


def git(*args):
    return subprocess.check_output(['git', '-C', ROOT, *args]).decode('latin-1')


def literals(rev):
    found = collections.Counter()
    where = {}
    for path in git('ls-tree', '-r', '--name-only', rev).split('\n'):
        if not path.endswith(('.cpp', '.h', '.c')) or not path.startswith(('src/', 'tests/', 'EntornVGI/')) \
                or 'third_party' in path or path.endswith(('.bak', '.TMP')) or 'ModelLoader' in path:
            continue
        text = git('show', f'{rev}:{path}').replace('\r\n', '\n')
        text = re.sub(r'(?m)^[ \t]*#[ \t]*include[^\n]*', '', text)
        for m in TOKEN.finditer(text):
            token = m.group(0)
            if token.startswith(('//', '/*')):
                continue
            found[token] += 1
            where.setdefault(token, path)
    return found, where


a, wa = literals(A)
b, wb = literals(B)
gone, new = a - b, b - a
print(f'{sum(a.values())} literals at {A}, {sum(b.values())} at {B}; removed {sum(gone.values())}, added {sum(new.values())}')
for label, counter, where in (('removed', gone, wa), ('added', new, wb)):
    for token, count in sorted(counter.items(), key=lambda x: where[x[0]]):
        print(f'{label} x{count} [{where[token]}] {" ".join(token.split())[:150]}')
