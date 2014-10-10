import unittest
import ctypes

import pypkgdepdb

class TestConfig(unittest.TestCase):
    def setUp(self):
        self.cfg = pypkgdepdb.Config()
    def tearDown(self):
        del self.cfg

    def MakeElf(self, dirname, basename,
                eclass, edata, eosabi,
                rpath, runpath, interp):
        elf = pypkgdepdb.Elf()
        self.assertIsNotNone(elf)

        elf.dirname     = dirname
        elf.basename    = basename
        elf.ei_class    = eclass
        elf.ei_data     = edata
        elf.ei_osabi    = eosabi
        elf.rpath       = rpath
        elf.runpath     = runpath
        elf.interpreter = interp

        self.assertEqual(elf.dirname     , dirname)
        self.assertEqual(elf.basename    , basename)
        self.assertEqual(elf.ei_class    , eclass)
        self.assertEqual(elf.ei_data     , edata)
        self.assertEqual(elf.ei_osabi    , eosabi)
        self.assertEqual(elf.rpath       , rpath)
        self.assertEqual(elf.runpath     , runpath)
        self.assertEqual(elf.interpreter , interp)

        return elf

    def test_elf(self):
        foo = self.MakeElf('/usr/lib', 'libfoo.so',
                           pypkgdepdb.ELF.CLASS64, pypkgdepdb.ELF.DATA2LSB,
                           pypkgdepdb.ELF.OSABI_NONE,
                           "/usr/lib:/usr/local/lib", None, "/lib/ld-elf.so")

        foo.needed.append('libbar1.so')
        foo.needed[1] = 'libbar2.so'
        with self.assertRaises(IndexError):
            foo.needed[3] = 'fail'
        with self.assertRaises(TypeError):
            foo.needed['foo'] = 'fail'
        self.assertEqual(len(foo.needed), 2)
        foo.needed.extend(['a', 'b', 'c'])
        self.assertEqual(len(foo.needed), 5)
        del foo.needed[3]
        self.assertEqual(len(foo.needed), 4)
        self.assertEqual(foo.needed[0], 'libbar1.so')
        self.assertEqual(foo.needed[1], 'libbar2.so')
        self.assertEqual(foo.needed[2], 'a')
        self.assertEqual(foo.needed[3], 'c')
        del foo.needed['a']
        self.assertEqual(len(foo.needed), 3)
        self.assertEqual(foo.needed[0], 'libbar1.so')
        self.assertEqual(foo.needed[1], 'libbar2.so')
        self.assertEqual(foo.needed[2], 'c')
        foo.needed.extend(['0', '1', '2', '3', '4', '5'])
        self.assertEqual(len(foo.needed), 9)
        self.assertEqual(foo.needed[0], 'libbar1.so')
        self.assertEqual(foo.needed[1], 'libbar2.so')
        self.assertEqual(foo.needed[2], 'c')
        for i in range(6):
            self.assertEqual(foo.needed[3+i], str(i))
        del foo.needed[2:4]
        self.assertEqual(len(foo.needed), 7)
        self.assertEqual(foo.needed[0], 'libbar1.so')
        self.assertEqual(foo.needed[1], 'libbar2.so')
        for i in range(1,6):
            self.assertEqual(foo.needed[1+i], str(i))
        del foo.needed[2:7:2]
        self.assertEqual(len(foo.needed), 4)
        self.assertEqual(foo.needed[0], 'libbar1.so')
        self.assertEqual(foo.needed[1], 'libbar2.so')
        self.assertEqual(foo.needed[2], '2')
        self.assertEqual(foo.needed[3], '4')
        del foo.needed[2:4]
        self.assertEqual(len(foo.needed), 2)
        self.assertEqual(foo.needed[0], 'libbar1.so')
        self.assertEqual(foo.needed[1], 'libbar2.so')

        bar = self.MakeElf('/usr/lib', 'libbar1.so',
                           pypkgdepdb.ELF.CLASS64, pypkgdepdb.ELF.DATA2LSB,
                           pypkgdepdb.ELF.OSABI_NONE,
                           "/usr/lib:/usr/local/lib", None, "/lib/ld-elf.so")
        bar32 = self.MakeElf('/usr/lib', 'libbar1.so',
                             pypkgdepdb.ELF.CLASS32, pypkgdepdb.ELF.DATA2LSB,
                             pypkgdepdb.ELF.OSABI_NONE,
                             "/usr/lib:/usr/local/lib", None, "/lib/ld-elf.so")
        abi = self.MakeElf('/usr/lib', 'libbar1.so',
                           pypkgdepdb.ELF.CLASS64, pypkgdepdb.ELF.DATA2LSB,
                           pypkgdepdb.ELF.OSABI_FREEBSD,
                           "/usr/lib:/usr/local/lib", None, "/lib/ld-elf.so")

        self.assertTrue (foo.can_use(bar,   True))
        self.assertFalse(foo.can_use(bar32, True))
        self.assertTrue (foo.can_use(abi,   False))
        self.assertFalse(foo.can_use(abi,   True))
        self.assertTrue (abi.can_use(foo,   False))
        self.assertFalse(abi.can_use(foo,   True))

        del foo
        del bar
        del bar32
        del abi

    def test_libpkgdepdb(self):
        elf = pypkgdepdb.Elf.load(".libs/libpkgdepdb.so", self.cfg)
        self.assertIsNotNone(elf)
        self.assertEqual(elf.basename, 'libpkgdepdb.so')

if __name__ == '__main__':
    unittest.main()
