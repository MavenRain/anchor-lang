#!/usr/bin/env python3
"""Regression tests for porting failures and kit ownership."""

import contextlib
import importlib.util
import io
import os
import pathlib
import shutil
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location(
    'port', pathlib.Path(__file__).with_name('port.py'))
PORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PORT)


class PortTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='anchor-port-')
        self.addCleanup(self.temp.cleanup)
        self.base = pathlib.Path(self.temp.name)
        self.source = self.base / 'source'
        self.kit = self.base / 'kit'
        self.kit.mkdir()
        original_root = PORT.ROOT
        self.addCleanup(setattr, PORT, 'ROOT', original_root)
        PORT.ROOT = self.source
        self.sources = {
            'src/main.c': b'// anchor-lang\nanchorc anchor_entry ANCHOR_MAX\n',
            'src/syntax.h': b'// anchor-lang\n',
            'test/parser-arms.anc': b'test anchorc prelude/Prelude.anc\n',
            'examples/programs/example.anc': b'program\n',
            'examples/mutants/mutant.anc': b'mutant\n',
            'prelude/Prelude.anc': b'domain\n',
            'tools/embed.c': b'build/prelude.c\n',
        }
        for name, data in self.sources.items():
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        for name in (*PORT.KIT_OWNED, 'docs/notes.md', 'build/output'):
            path = self.kit / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'kit-owned sentinel\n')

    def run_port(self, mode, kit=None):
        output, error = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(error):
            code = PORT.main([mode, str(kit or self.kit)])
        return code, output.getvalue(), error.getvalue()

    def snapshot(self, directory):
        return {path.relative_to(directory).as_posix(): path.read_bytes()
                for path in directory.rglob('*') if path.is_file()}

    def test_roundtrip_preserves_owned_files_and_renames(self):
        owned = self.snapshot(self.kit)
        self.assertEqual(self.run_port('--write')[0], 0)
        self.assertEqual(self.run_port('--check')[0], 0)
        self.assertEqual((self.kit / 'src/main.c').read_bytes(),
                         b'// {{LANG}}\nlangc lang_entry LANG_MAX\n')
        self.assertEqual((self.kit / 'test/parser-arms.lang').read_bytes(),
                         b'test langc domain/domain.lang\n')
        self.assertEqual((self.kit / 'gen/embed.c').read_bytes(), b'build/domain.c\n')
        self.assertEqual({name: (self.kit / name).read_bytes() for name in owned},
                         owned)

    def test_check_reports_diff_missing_and_extra(self):
        self.assertEqual(self.run_port('--write')[0], 0)
        (self.kit / 'src/main.c').write_bytes(b'changed\n')
        (self.kit / 'src/syntax.h').unlink()
        (self.kit / 'unexpected').write_bytes(b'extra\n')
        before = self.snapshot(self.kit)
        code, output, _ = self.run_port('--check')
        self.assertEqual(code, 1)
        self.assertIn('differs src/main.c', output)
        self.assertIn('missing src/syntax.h', output)
        self.assertIn('extra unexpected', output)
        self.assertEqual(self.snapshot(self.kit), before)

    def test_missing_source_directory_refuses_before_writes(self):
        shutil.rmtree(self.source / 'src')
        before = self.snapshot(self.kit)
        for mode in ('--write', '--check'):
            with self.subTest(mode=mode):
                code, _, error = self.run_port(mode)
                self.assertEqual(code, 2)
                self.assertIn('no source directory src', error)
                self.assertEqual(self.snapshot(self.kit), before)

    def test_empty_source_group_refuses(self):
        (self.source / 'examples/programs/example.anc').unlink()
        before = self.snapshot(self.kit)
        code, _, error = self.run_port('--write')
        self.assertEqual(code, 2)
        self.assertIn('no source files examples/programs/*.anc', error)
        self.assertEqual(self.snapshot(self.kit), before)

    def test_missing_single_source_refuses_before_writes(self):
        (self.source / 'prelude/Prelude.anc').unlink()
        before = self.snapshot(self.kit)
        self.assertEqual(self.run_port('--write')[0], 2)
        self.assertEqual(self.snapshot(self.kit), before)

    def test_symlink_to_owned_file_is_neither_read_nor_written(self):
        (self.kit / 'src').mkdir()
        target = self.kit / 'src/main.c'
        target.symlink_to(self.kit / 'README.md')
        before = self.snapshot(self.kit)
        for mode in ('--write', '--check'):
            with self.subTest(mode=mode):
                code, _, error = self.run_port(mode)
                self.assertEqual(code, 2)
                self.assertIn('symlink kit path src/main.c', error)
                self.assertEqual(self.snapshot(self.kit), before)
                self.assertTrue(target.is_symlink())

    def test_symlink_parent_to_owned_directory_is_preserved(self):
        (self.kit / 'src').symlink_to(self.kit / 'docs', target_is_directory=True)
        before = self.snapshot(self.kit)
        self.assertEqual(self.run_port('--write')[0], 2)
        self.assertEqual(self.snapshot(self.kit), before)
        self.assertFalse((self.kit / 'docs/main.c').exists())

    def test_dangling_symlink_cannot_create_external_file(self):
        (self.kit / 'src').mkdir()
        external = self.base / 'external'
        (self.kit / 'src/main.c').symlink_to(external)
        self.assertEqual(self.run_port('--write')[0], 2)
        self.assertFalse(external.exists())

    def test_hardlink_to_owned_file_is_preserved(self):
        (self.kit / 'src').mkdir()
        os.link(self.kit / 'README.md', self.kit / 'src/main.c')
        before = self.snapshot(self.kit)
        self.assertEqual(self.run_port('--write')[0], 2)
        self.assertEqual(self.snapshot(self.kit), before)

    def test_source_root_cannot_be_its_own_kit(self):
        before = self.snapshot(self.source)
        self.assertEqual(self.run_port('--write', self.source)[0], 2)
        self.assertEqual(self.snapshot(self.source), before)

    def test_duplicate_mapping_refuses_before_writes(self):
        (self.source / 'test/parser-arms.lang').write_bytes(b'conflict\n')
        before = self.snapshot(self.kit)
        code, _, error = self.run_port('--write')
        self.assertEqual(code, 2)
        self.assertIn('duplicate kit path test/parser-arms.lang', error)
        self.assertEqual(self.snapshot(self.kit), before)

    def test_obstructed_target_refuses_before_writes(self):
        (self.kit / 'src').write_bytes(b'blocked\n')
        before = self.snapshot(self.kit)
        code, _, error = self.run_port('--write')
        self.assertEqual(code, 2)
        self.assertIn('non-directory kit path src', error)
        self.assertEqual(self.snapshot(self.kit), before)

    def test_usage_and_missing_kit(self):
        output = io.StringIO()
        with contextlib.redirect_stderr(output):
            self.assertEqual(PORT.main([]), 2)
            self.assertEqual(PORT.main(['--invalid', str(self.kit)]), 2)
        self.assertEqual(self.run_port('--write', self.base / 'missing')[0], 2)


if __name__ == '__main__':
    unittest.main()
