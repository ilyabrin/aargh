#!/usr/bin/env python3
"""Checks that the documentation matches the code.

    python3 tests/docs/check_docs.py [--cc CC]     (make docs-check)

1. Code: every C block in README.md that is marked
       <!-- docs-check: source=tests/docs/NAME.c -->
   appears line by line, in order, in that file, and the file compiles.
2. Output: every "$ ./prog args" line in a console block runs the real
   program and must print exactly what follows it. README.md marks each
   block with <!-- docs-check: program=NAME -->; examples/README.md maps
   ./wc, ./logship and ./pkg to the example programs.
3. Names: every public argh_/ARGH_ name in argh.h is in README.md, and
   README.md names nothing that argh.h lacks (outside "Upgrading from 0.1").
4. llms.txt: its C snippets compile.

The markers are HTML comments, invisible on GitHub. In README.md every c and
console block needs one, so a lost marker can't quietly stop a check; blocks
that can't be checked, such as excerpts and pseudo-code, say so with
<!-- docs-check: skip -->.
"""
import argparse
import os
import re
import shlex
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
EXE = '.exe' if os.name == 'nt' else ''
CFLAGS = ['-std=c99', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter']

# Programs the docs show, by the name used in markers and "$ ./name"
PROGRAMS = {
    'mytool': (['tests/docs/mytool.c'], []),
    'tool': (['tests/docs/tool.c'], []),
    'sizetool': (['tests/docs/sizetool.c'], []),
    'export': (['tests/docs/export.c'], []),
    'convert': (['tests/docs/convert.c'], []),
    'convert_broken': (['tests/docs/convert.c'], ['-DBROKEN']),
    'guide': (['tests/docs/guide.c'], []),
    'mcu': (['tests/docs/mcu.c'], []),
    'wc': (['examples/wc.c'], []),
    'logship': (['examples/logship.c'], []),
    'pkg': (['examples/pkg/main.c', 'examples/pkg/install.c', 'examples/pkg/remote.c', 'examples/pkg/exec.c'], []),
}

# Programs that are run with these arguments and must exit with 0
RUNS = [('guide', ['--help']), ('guide', ['-o', 'out.txt', 'a.txt', 'b.txt']), ('mcu', ['-v'])]

# examples/README.md uses the program names directly
EXAMPLES_MAP = {'./wc': 'wc', './logship': 'logship', './pkg': 'pkg'}

failures = []


def fail(message):
    failures.append(message)
    print('FAIL ' + message)


def read(path):
    with open(os.path.join(ROOT, path), encoding='utf-8') as f:
        return f.read().replace('\r\n', '\n')


def line_of(text, pos):
    return text.count('\n', 0, pos) + 1


def build(cc, out):
    binaries = {}
    for name, (sources, flags) in PROGRAMS.items():
        exe = os.path.join(out, name + EXE)
        cmd = [cc] + CFLAGS + flags + ['-I', ROOT, '-o', exe] + [os.path.join(ROOT, s) for s in sources]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            fail('%s does not compile:\n%s' % (name, r.stdout + r.stderr))
        else:
            binaries[name] = exe
    return binaries


def blocks(text):
    """(kind, body, marker, line) for every fenced block; marker is the
    docs-check comment right before it, if any"""
    for m in re.finditer(r'(?:<!-- docs-check: ([^>]*?) -->\n)?```(\w*)\n(.*?)```', text, re.S):
        yield m.group(2), m.group(3), m.group(1), line_of(text, m.start(2) if m.group(1) else m.start())


def check_marked(path, text):
    """Every c and console block carries a docs-check marker"""
    for kind, body, marker, line in blocks(text):
        if kind in ('c', 'console') and not marker:
            fail('%s:%d: %s block without a docs-check marker (use skip if it cannot be checked)' % (path, line, kind))


def check_code(path, text):
    count = 0
    for kind, body, marker, line in blocks(text):
        if kind != 'c' or not marker or marker == 'skip':
            continue
        m = re.fullmatch(r'source=(\S+)', marker)
        if not m:
            fail('%s:%d: unknown marker %r' % (path, line, marker))
            continue
        source = m.group(1)
        have = [l.strip() for l in read(source).split('\n') if l.strip()]
        want = [l.strip() for l in body.split('\n') if l.strip()]
        i = 0
        for w in want:
            while i < len(have) and have[i] != w:
                i += 1
            if i == len(have):
                fail('%s:%d: this line is not in %s (in this order): %s' % (path, line, source, w))
                break
            i += 1
        count += 1
    return count


def run(exe, args, cwd=ROOT):
    r = subprocess.run([exe] + args, cwd=cwd, capture_output=True, text=True)
    return r.returncode, (r.stdout + r.stderr).replace('\r\n', '\n').rstrip('\n')


def check_console(path, text, binaries, mapping=None, cwd=ROOT):
    count = 0
    for kind, body, marker, line in blocks(text):
        if kind != 'console':
            continue
        program = None
        if marker == 'skip':
            continue
        if marker:
            m = re.fullmatch(r'program=(\S+)', marker)
            if not m or m.group(1) not in PROGRAMS:
                fail('%s:%d: unknown marker %r' % (path, line, marker))
                continue
            program = m.group(1)
        elif not mapping:
            continue
        lines = body.split('\n')
        last_rc = None
        i = 0
        while i < len(lines):
            if not lines[i].startswith('$ '):
                i += 1
                continue
            cmd = lines[i][2:]
            j = i + 1
            while j < len(lines) and not lines[j].startswith('$ '):
                j += 1
            expected = '\n'.join(lines[i + 1:j]).rstrip('\n')
            at = line + 1 + i
            i = j
            if cmd == 'echo $?':
                if last_rc is None or expected != str(last_rc):
                    fail('%s:%d: "echo $?" shows %s, the exit code was %s' % (path, at, expected, last_rc))
                continue
            argv = shlex.split(cmd)
            name = program if program else mapping.get(argv[0])
            if not name:
                continue
            if name not in binaries:
                fail('%s:%d: %s was not built' % (path, at, name))
                continue
            last_rc, got = run(binaries[name], argv[1:], cwd)
            count += 1
            if got != expected:
                fail('%s:%d: output of "%s" differs\n--- documented\n%s\n--- real\n%s' % (path, at, cmd, expected, got))
    return count


def check_names():
    header = read('argh.h')
    public = header[:header.index('#endif /* ARGH_H_INCLUDED */')]
    readme = read('README.md')
    current = readme[:readme.index('## Upgrading from 0.1')] + readme[readme.index('## Examples', readme.index('## Upgrading from 0.1')):]
    ident = r'\b(?:argh_[a-z_]+|ARGH_[A-Z0-9_]+)\b'
    names = {n for n in re.findall(ident, public) if '__' not in n and n != 'ARGH_H_INCLUDED'}
    # argh_init is renamed after the settings: the comment shows a sample name
    names = {n for n in names if not n.startswith('argh_init_settings_')}
    for n in sorted(names):
        if not re.search(r'\b' + n + r'\b', readme):
            fail('README.md does not mention %s from argh.h' % n)
    for n in sorted(set(re.findall(ident, current))):
        # Names ending in _ are patterns such as ARGH_E_* or ARGH__K_*
        if n.endswith('_') or n.startswith('argh_init_settings_'):
            continue
        if not re.search(r'\b' + n + r'\b', header):
            fail('README.md mentions %s, which argh.h does not have' % n)
    return len(names)


def check_llms(cc, out):
    text = read('llms.txt')
    snippets = re.findall(r'```c\n(.*?)```', text, re.S)
    if not snippets:
        fail('llms.txt has no C snippets')
        return 0
    # The first one is a whole program
    sources = [('llms_program.c', snippets[0])]
    # The others are fragments: file-scope parts first, then statements
    top, body = [], []
    for s in snippets[1:]:
        cut = re.search(r'^(?:/\* in main|argh_[a-z_]+\(&p)', s, re.M)
        if cut:
            top.append(s[:cut.start()])
            body.append(s[cut.start():])
        else:
            top.append(s)
    wrapper = ('#include <stdlib.h>\n#define ARGH_IMPLEMENTATION\n#include "argh.h"\n\n'
               'static bool json, yaml, use_stdin, a, b;\n'
               'static const char *input, *tls_key, *tls_cert;\n'
               'static int port;\n\n' + '\n'.join(top) +
               '\nint main(int argc, char **argv)\n{\n    argh_parser p;\n    argh_init(&p, "tool", NULL);\n' +
               '\n'.join(body) + '\n    return argh_parse(&p, argc, argv) ? 0 : argh_exit_code(&p);\n}\n')
    sources.append(('llms_fragments.c', wrapper))
    for name, code in sources:
        path = os.path.join(out, name)
        with open(path, 'w') as f:
            f.write(code)
        exe = os.path.splitext(path)[0] + EXE
        r = subprocess.run([cc] + CFLAGS + ['-I', ROOT, '-o', exe, path], capture_output=True, text=True)
        if r.returncode != 0:
            fail('llms.txt: %s does not compile:\n%s' % (name, r.stdout + r.stderr))
    return len(snippets)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--cc', default=os.environ.get('CC') or 'cc')
    cc = ap.parse_args().cc

    with tempfile.TemporaryDirectory() as out:
        binaries = build(cc, out)
        readme = read('README.md')
        examples = read('examples/README.md')
        check_marked('README.md', readme)
        code = check_code('README.md', readme)
        output = check_console('README.md', readme, binaries)
        # Its commands run in examples/, next to the programs and data/
        output += check_console('examples/README.md', examples, binaries, EXAMPLES_MAP, os.path.join(ROOT, 'examples'))
        for name, args in RUNS:
            if name in binaries:
                rc, got = run(binaries[name], args)
                if rc != 0:
                    fail('%s %s exits with %d:\n%s' % (name, ' '.join(args), rc, got))
        names = check_names()
        llms = check_llms(cc, out)

    print('%d code blocks, %d commands, %d public names, %d llms.txt snippets checked' % (code, output, names, llms))
    if failures:
        print('%d problem(s) found' % len(failures))
        return 1
    print('docs match the code')
    return 0


if __name__ == '__main__':
    sys.exit(main())
