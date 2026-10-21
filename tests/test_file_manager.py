"""Real temporary filesystem tests for the native backend; requires gcc and zlib."""
import ctypes
import errno
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import zipfile


class NativeFiles(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix='wolo-files-build-')
        binary = Path(cls.build.name) / 'files.so'
        root = Path(__file__).resolve().parents[1]
        subprocess.run(['gcc', '-D_GNU_SOURCE', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
                        str(root/'src/file_manager.c'), '-lz', '-o', str(binary)], check=True)
        cls.lib = ctypes.CDLL(str(binary), use_errno=True)
        cls.lib.fm_read.argtypes=[ctypes.c_char_p,ctypes.c_char_p,ctypes.POINTER(ctypes.c_void_p),ctypes.POINTER(ctypes.c_size_t),ctypes.c_char_p]
        cls.lib.fm_write.argtypes=[ctypes.c_char_p,ctypes.c_char_p,ctypes.c_char_p,ctypes.c_size_t,ctypes.c_char_p]
        cls.lib.free.argtypes=[ctypes.c_void_p]
        for name, args in [('fm_copy', [ctypes.c_char_p]*3+[ctypes.c_int]),
                           ('fm_delete', [ctypes.c_char_p]*2), ('fm_unzip', [ctypes.c_char_p]*3)]:
            getattr(cls.lib, name).argtypes = args

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='wolo-files-test-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root/'source/sub').mkdir(parents=True)
        (self.root/'source/sub/file.txt').write_text('keep me')

    def call(self, name, *args):
        values = [str(self.root).encode()] + [a.encode() if isinstance(a, str) else a for a in args]
        return getattr(self.lib, 'fm_'+name)(*values)

    def read_file(self,path):
        pointer=ctypes.c_void_p(); size=ctypes.c_size_t(); revision=ctypes.create_string_buffer(160)
        result=self.lib.fm_read(str(self.root).encode(),path.encode(),ctypes.byref(pointer),ctypes.byref(size),revision)
        data=ctypes.string_at(pointer,size.value) if result==0 else None
        if pointer.value: self.lib.free(pointer)
        return result,data,revision.value

    def write_file(self,path,data,revision):
        return self.lib.fm_write(str(self.root).encode(),path.encode(),data,len(data),revision)

    def test_revision_checked_saves_and_binary_uploads(self):
        target=self.root/'settings.ini'; target.write_bytes(b'first\r\n'); target.chmod(0o640)
        result,data,revision=self.read_file('settings.ini')
        self.assertEqual((result,data),(0,b'first\r\n'))
        self.assertEqual(self.write_file('settings.ini',b'second\r\n',revision),0)
        self.assertEqual(target.read_bytes(),b'second\r\n')
        self.assertEqual(target.stat().st_mode & 0o777,0o640)
        self.assertEqual(self.write_file('settings.ini',b'outdated',revision),-1)
        self.assertEqual(ctypes.get_errno(),errno.ESTALE)
        self.assertEqual(target.read_bytes(),b'second\r\n')
        _,_,revision=self.read_file('settings.ini')
        # Same-size edits within the same timestamp second must also conflict.
        target.write_bytes(b'third!\r\n')
        self.assertEqual(self.write_file('settings.ini',b'outdated',revision),-1)
        self.assertEqual(ctypes.get_errno(),errno.ESTALE)
        data=bytes(range(256))*500
        self.assertEqual(self.write_file('new.bin',data,b'new'),0)
        self.assertEqual(self.read_file('new.bin')[1],data)
        self.assertEqual(self.write_file('new.bin',b'refused',b'new'),-1)
        self.assertEqual(ctypes.get_errno(),errno.EEXIST)
        self.assertEqual(self.write_file('empty',b'',b'new'),0)
        self.assertEqual(self.read_file('empty')[1],b'')
        self.assertFalse(list(self.root.glob('.poldrive-save-*')))

    def test_file_transfer_refuses_links_special_files_and_limits(self):
        (self.root/'link').symlink_to(self.root/'source/sub/file.txt')
        for path in ('link','source','../outside',''):
            self.assertEqual(self.read_file(path)[0],-1,path)
        self.assertEqual(self.write_file('link',b'x',b'new'),-1)
        self.assertEqual(self.write_file('../outside',b'x',b'new'),-1)
        with (self.root/'large').open('wb') as stream: stream.truncate(8*1024*1024+1)
        self.assertEqual(self.read_file('large')[0],-1)
        self.assertEqual(ctypes.get_errno(),errno.EFBIG)
        os.mkfifo(self.root/'pipe')
        self.assertEqual(self.read_file('pipe')[0],-1)

    def test_copy_move_delete_and_conflicts(self):
        self.assertEqual(self.call('copy', 'source', 'copied', 0), 0)
        self.assertEqual((self.root/'copied/sub/file.txt').read_text(), 'keep me')
        self.assertEqual(self.call('copy', 'source', 'copied', 0), -1)
        self.assertEqual(ctypes.get_errno(), errno.EEXIST)
        self.assertEqual(self.call('copy', 'copied', 'moved', 1), 0)
        self.assertFalse((self.root/'copied').exists())
        self.assertEqual(self.call('delete', 'moved'), 0)
        self.assertFalse((self.root/'moved').exists())
        self.assertTrue((self.root/'source/sub/file.txt').exists())

    def test_refuse_self_children_traversal_and_root(self):
        for dest in ['source', 'source/sub/child', '../escape', '/absolute']:
            self.assertEqual(self.call('copy', 'source', dest, 0), -1, dest)
        for path in ['', '..', 'source/../../escape']:
            self.assertEqual(self.call('delete', path), -1, path)
        self.assertEqual(self.call('copy', '../escape', 'output', 0), -1)

    def test_symlinks_and_partial_copy_cleanup(self):
        os.symlink(self.root/'source', self.root/'link')
        self.assertEqual(self.call('copy', 'link', 'output', 0), -1)
        self.assertEqual(self.call('delete', 'link/sub/file.txt'), -1)
        os.symlink('/etc/passwd', self.root/'source/link')
        self.assertEqual(self.call('copy', 'source', 'partial', 0), -1)
        self.assertFalse((self.root/'partial').exists())
        self.assertTrue((self.root/'source/sub/file.txt').exists())

    def archive(self, name='nested/data.txt', method=zipfile.ZIP_DEFLATED, payload=b'hello' * 20000):
        with zipfile.ZipFile(self.root/'test.zip', 'w', method) as archive:
            archive.writestr(name, payload)

    def test_unzip_stored_deflated_crc_and_collision(self):
        for method in [zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED]:
            self.archive(method=method)
            self.assertEqual(self.call('unzip', 'test.zip', 'output'), 0)
            self.assertEqual((self.root/'output/nested/data.txt').read_bytes(), b'hello'*20000)
            self.assertEqual(self.call('unzip', 'test.zip', 'output'), -1)
            self.assertEqual(self.call('delete', 'output'), 0)
        data = bytearray((self.root/'test.zip').read_bytes())
        central = data.index(b'PK\x01\x02')
        data[central+16] ^= 1
        (self.root/'test.zip').write_bytes(data)
        self.assertEqual(self.call('unzip', 'test.zip', 'output'), -1)
        self.assertFalse((self.root/'output').exists())

    def test_zip_unsafe_paths_and_duplicates(self):
        for name in ['../escape', '/absolute', 'a/../../escape', 'a\\escape', 'a//escape', 'C:/escape', './escape']:
            self.archive(name)
            self.assertEqual(self.call('unzip', 'test.zip', 'output'), -1, name)
            self.assertFalse((self.root/'output').exists(), name)
        with zipfile.ZipFile(self.root/'test.zip', 'w') as archive:
            archive.writestr('same', 'first')
            archive.writestr('same', 'second')
        self.assertEqual(self.call('unzip', 'test.zip', 'output'), -1)
        self.assertFalse((self.root/'output').exists())

    def test_zip_truncated_symlink_and_bomb(self):
        self.archive()
        data = (self.root/'test.zip').read_bytes()
        for length in [0, 21, len(data)-1]:
            (self.root/'test.zip').write_bytes(data[:length])
            self.assertEqual(self.call('unzip', 'test.zip', 'output'), -1)
            self.assertFalse((self.root/'output').exists())
        entry = zipfile.ZipInfo('link'); entry.create_system = 3; entry.external_attr = 0o120777 << 16
        with zipfile.ZipFile(self.root/'test.zip', 'w') as archive:
            archive.writestr(entry, '../outside')
        self.assertEqual(self.call('unzip', 'test.zip', 'output'), -1)
        self.archive()
        data = bytearray((self.root/'test.zip').read_bytes()); central=data.index(b'PK\x01\x02')
        data[central+24:central+28] = (513*1024*1024).to_bytes(4, 'little')
        (self.root/'test.zip').write_bytes(data)
        self.assertEqual(self.call('unzip', 'test.zip', 'output'), -1)
        self.assertFalse((self.root/'output').exists())

    def test_zip_forged_zero_size_does_not_succeed_with_truncated_output(self):
        self.archive(payload=b'not empty')
        data = bytearray((self.root/'test.zip').read_bytes()); central=data.index(b'PK\x01\x02')
        data[central+16:central+20] = bytes(4)  # CRC of an empty file
        data[central+24:central+28] = bytes(4)  # Actual inflation is not empty
        (self.root/'test.zip').write_bytes(data)
        self.assertEqual(self.call('unzip', 'test.zip', 'output'), -1)
        self.assertFalse((self.root/'output').exists())


if __name__ == '__main__':
    unittest.main()
