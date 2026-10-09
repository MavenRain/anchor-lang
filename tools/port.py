#!/usr/bin/env python3
"""Port the anchor-lang compiler to the lang-template kit.

Usage: python3 -I tools/port.py --write KIT
       python3 -I tools/port.py --check KIT

KIT is the kit directory, lang-template/hosts/tcc-evm-anchor. The source
is the working tree of this repository. Each mapped kit file is its source
file with the M5 renames (SPEC section 10). --write writes the mapped
files. --check prints each mapped path that differs, each missing file and
each extra file, and exits 1 if there is one; else it prints the count of
mapped files and exits 0. Neither mode writes or reads a kit-owned file.
Exit 2: a usage error, incomplete sources, an unsafe kit path, or an I/O
error. Validation runs before writing any file. KIT must differ from the
source root; mapped paths must not use symlinks or multiply linked files.
"""

import functools
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

# Kit files that M5 wrote by hand. The script never writes them.
KIT_OWNED = ('.gitignore', 'Makefile', 'README.md', 'FORMERS.md',
             'test/gate.sh')
KIT_OWNED_DIRS = ('docs/',)

# Kit directories that --check does not look in (the make output).
KIT_IGNORED_DIRS = ('build/',)

# Each source directory, its file pattern, and the kit directory.
GROUPS = (
    ('src', '*', 'src'),
    ('test', '*', 'test'),
    ('examples/programs', '*.anc', 'examples/programs'),
    ('examples/mutants', '*.anc', 'examples/mutants'),
)

# Single source files and their kit paths.
SINGLES = (
    ('prelude/Prelude.anc', 'domain/domain.lang'),
    ('tools/embed.c', 'gen/embed.c'),
)

# The M5 renames, in this order. The domain word "anchor" stays.
RENAMES = (
    (re.compile(rb'prelude/Prelude\.anc\b'), b'domain/domain.lang'),
    (re.compile(rb'\bPrelude\.anc\b'), b'domain/domain.lang'),
    (re.compile(rb'\btools/embed\.c\b'), b'gen/embed.c'),
    (re.compile(rb'\bbuild/prelude\.c\b'), b'build/domain.c'),
    (re.compile(rb'\banchorc\b'), b'langc'),
    (re.compile(rb'\banchor_'), b'lang_'),
    (re.compile(rb'\bANCHOR_'), b'LANG_'),
    (re.compile(rb'\.anc\b'), b'.lang'),
)

# Kit files whose first line names the language as {{LANG}}.
PLACEHOLDER_FILES = ('src/main.c', 'src/syntax.h')


def kit_name(name):
    return re.sub(r'\.anc$', '.lang', name)


def mapping():
    """The (source path, kit path) pairs, sorted by kit path."""
    grouped = (
        (f'{src}/{path.name}', f'{kit}/{kit_name(path.name)}')
        for src, pattern, kit in GROUPS
        for path in (ROOT / src).glob(pattern)
        if path.is_file() and not path.name.startswith('.')
    )
    return sorted((*grouped, *SINGLES), key=lambda pair: pair[1])


def kit_owned(path):
    return path in KIT_OWNED or path.startswith(KIT_OWNED_DIRS)


def placeholder(kit_path, data):
    first, newline, rest = data.partition(b'\n')
    named = first.replace(b'anchor-lang', b'{{LANG}}') + newline + rest
    return named if kit_path in PLACEHOLDER_FILES else data


def port(src, kit_path):
    """The bytes of the kit file: the source with the renames."""
    renamed = functools.reduce(lambda data, rule: rule[0].sub(rule[1], data),
                               RENAMES, (ROOT / src).read_bytes())
    return placeholder(kit_path, renamed)


def kit_files(kit):
    relative = (path.relative_to(kit).as_posix()
                for path in kit.rglob('*') if path.is_file())
    return sorted(path for path in relative
                  if not path.startswith(KIT_IGNORED_DIRS))


def source_errors():
    errors = []
    for src, pattern, _ in GROUPS:
        directory = ROOT / src
        if not directory.is_dir():
            errors.append(f'port: no source directory {src}')
        elif not any(path.is_file() and not path.name.startswith('.')
                     for path in directory.glob(pattern)):
            errors.append(f'port: no source files {src}/{pattern}')
    return errors


def path_errors(kit, pairs):
    errors = []
    mapped = set()
    for _, kit_path in pairs:
        if kit_path in mapped:
            errors.append(f'port: duplicate kit path {kit_path}')
        mapped.add(kit_path)
        if kit_owned(kit_path):
            errors.append(f'port: kit-owned path {kit_path}')
        target = kit / kit_path
        current = target
        while current != kit:
            relative = current.relative_to(kit).as_posix()
            if current.is_symlink():
                errors.append(f'port: symlink kit path {relative}')
                break
            if current.exists():
                if current == target:
                    if not current.is_file():
                        errors.append(f'port: non-file kit path {relative}')
                    elif current.stat().st_nlink != 1:
                        errors.append(f'port: multiply linked kit path {relative}')
                elif not current.is_dir():
                    errors.append(f'port: non-directory kit path {relative}')
                    break
            current = current.parent
    return sorted(set(errors))


def write_one(kit, kit_path, data):
    target = kit / kit_path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)
    return 1


def write(kit, pairs):
    files = [(kit_path, port(src, kit_path)) for src, kit_path in pairs]
    count = sum(write_one(kit, kit_path, data) for kit_path, data in files)
    print(f'port write: {count} files')
    return 0


def check(kit, pairs):
    mapped = {kit_path for _, kit_path in pairs}
    missing = [kit_path for _, kit_path in pairs
               if not (kit / kit_path).is_file()]
    differs = [kit_path for src, kit_path in pairs
               if (kit / kit_path).is_file()
               and (kit / kit_path).read_bytes() != port(src, kit_path)]
    extra = [path for path in kit_files(kit)
             if path not in mapped and not kit_owned(path)]
    report = ([f'differs {path}' for path in differs]
              + [f'missing {path}' for path in missing]
              + [f'extra {path}' for path in extra])
    print('\n'.join(report) if report
          else f'port check: {len(pairs)} files match')
    return 1 if report else 0


def main(argv):
    modes = {'--write': write, '--check': check}
    if len(argv) != 2 or argv[0] not in modes:
        print('usage: python3 -I tools/port.py --write KIT | --check KIT',
              file=sys.stderr)
        return 2
    try:
        kit = pathlib.Path(argv[1]).resolve()
        pairs = mapping()
        errors = source_errors() \
            + ([f'port: no kit directory {kit}'] if not kit.is_dir() else []) \
            + ([f'port: kit is the source root {kit}']
               if kit == ROOT.resolve() else []) \
            + [f'port: no source file {src}' for src, _ in pairs
               if not (ROOT / src).is_file()] \
            + path_errors(kit, pairs)
        if errors:
            print('\n'.join(errors), file=sys.stderr)
            return 2
        return modes[argv[0]](kit, pairs)
    except OSError as error:
        print(f'port: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
