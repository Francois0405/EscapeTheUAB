"""Split a C++ translation unit into top-level items (descending into namespaces)."""
import re
import sys

BS = chr(92)


def strip(text):
    """Blank comments and string contents, keeping every offset and newline."""
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if text.startswith('//', i):
            j = text.find('\n', i)
            j = n if j < 0 else j
            out.append(' ' * (j - i))
            i = j
        elif text.startswith('/*', i):
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r'[^\n]', ' ', text[i:j]))
            i = j
        elif text.startswith('R"', i) and (i == 0 or text[i - 1] in 'uUL8' or not (text[i - 1].isalnum() or text[i - 1] == '_')):
            delimiter = text[i + 2:text.index('(', i + 2)]
            j = text.index(')' + delimiter + '"', i + 2) + len(delimiter) + 2
            out.append('R"' + re.sub(r'[^\n]', ' ', text[i + 2:j - 1]) + '"')
            i = j
        elif c in '"\'':
            j = i + 1
            while j < n and text[j] != c and text[j] != '\n':
                j += 2 if text[j] == BS else 1
            out.append(c + ' ' * (j - i - 1) + (c if j < n else ''))
            i = j + 1
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def load(path):
    raw = open(path, 'rb').read()
    try:
        return raw.decode('utf-8-sig').replace('\r\n', '\n')
    except UnicodeDecodeError:
        return raw.decode('cp1252').replace('\r\n', '\n')


def items(text):
    """Return (stripped, [(kind, start, end, namespaces)]). Items are contiguous and cover text."""
    s = strip(text)
    res = []
    i = 0
    n = len(s)
    ns = []
    start = 0
    depth = 0
    par = 0
    while i < n:
        c = s[i]
        if depth == 0 and c == '#' and s[s.rfind('\n', 0, i) + 1:i].strip() == '':
            j = s.find('\n', i)
            j = n if j < 0 else j
            while text[j - 1] == BS:
                j = s.find('\n', j + 1)
            res.append(('pp', start, j + 1, tuple(ns)))
            i = j + 1
            start = i
            continue
        if c == '(':
            par += 1
        elif c == ')':
            par -= 1
        elif c == '{':
            if depth == 0:
                m = re.search(r'\bnamespace(?:\s+([\w:]+))?\s*$', s[start:i])
                if m:
                    ns.append(m.group(1) or '')
                    res.append(('nsopen', start, i + 1, tuple(ns)))
                    i += 1
                    start = i
                    continue
            depth += 1
        elif c == '}':
            if depth == 0:
                res.append(('nsclose', start, i + 1, tuple(ns)))
                ns.pop()
                i += 1
                start = i
                continue
            depth -= 1
            if depth == 0:
                head = s[start:i]
                h = head[:head.find('{')]
                paren = h.find('(')
                if (paren >= 0 and not re.search(r'(?<![=!<>+\-*/])=(?!=)', h[:paren])
                        and not re.match(r'\s*(struct|class|enum|union)\b', h)):
                    res.append(('func', start, i + 1, tuple(ns)))
                    start = i + 1
        elif c == ';' and depth == 0 and par == 0:
            res.append(('decl', start, i + 1, tuple(ns)))
            start = i + 1
        i += 1
    res.append(('tail', start, n, tuple(ns)))
    return s, res


def func_name(s, a, b):
    h = s[a:b]
    h = h[:h.find('(')].split()
    return h[-1].lstrip('*&') if h else '?'


if __name__ == '__main__':
    t = load(sys.argv[1])
    s, res = items(t)
    show_all = len(sys.argv) > 2
    for k, a, b, ns in res:
        body = s[a:b]
        if not body.strip():
            continue
        a2 = a + len(body) - len(body.lstrip())
        l1 = t.count('\n', 0, a2) + 1
        l2 = t.count('\n', 0, b) + 1
        tag = ' [' + '::'.join(x or 'anon' for x in ns) + ']' if ns else ''
        if k == 'func':
            if not show_all:
                print(f"{l1}-{l2} {func_name(s, a, b)}{tag};", end=' ')
        elif show_all and k != 'pp':
            print(f"{l1} {k}{tag}: {' '.join(t[a2:b].split())[:110]}")
    print()
