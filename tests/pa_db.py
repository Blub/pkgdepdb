import unittest
import ctypes

import pypkgdepdb

class TestConfig(unittest.TestCase):
    def setUp(self):
        self.cfg = pypkgdepdb.Config()
        self.cfg.quiet = True
        self.cfg.verbosity = 0
        self.cfg.log_level = pypkgdepdb.LogLevel.Error
    def tearDown(self):
        del self.cfg

    def MakeElf(self, dirname, basename,
                eclass=pypkgdepdb.ELF.CLASS64,
                edata=pypkgdepdb.ELF.DATA2LSB,
                eosabi=pypkgdepdb.ELF.OSABI_FREEBSD,
                rpath=None,
                runpath=None,
                interp=None):
        elf = pypkgdepdb.Elf()
        elf.dirname     = dirname
        elf.basename    = basename
        elf.ei_class    = eclass
        elf.ei_data     = edata
        elf.ei_osabi    = eosabi
        elf.rpath       = rpath
        elf.runpath     = runpath
        elf.interpreter = interp
        return elf

    def MakePkg(self, name, version, description,
                depends=[], optdepends=[], makedepends=[], checkdepends=[],
                conflicts=[], replaces=[], provides=[], pkgbase='',
                groups=[],
                check=False):
        pkg = pypkgdepdb.Package()
        pkg.name        = name
        pkg.version     = version
        pkg.description = description
        pkg.pkgbase     = pkgbase
        pkg.depends.extend(depends)
        pkg.optdepends.extend(optdepends)
        pkg.makedepends.extend(makedepends)
        pkg.checkdepends.extend(checkdepends)
        pkg.conflicts.extend(conflicts)
        pkg.replaces.extend(replaces)
        pkg.provides.extend(provides)
        pkg.groups.extend(groups)
        return pkg

    def test_dbcore(self):
        db = pypkgdepdb.DB(self.cfg)
        self.assertIsNotNone(db)

        self.assertEqual(len(db.library_path), 0)
        lst = [ '/lib', '/usr/lib', 'a', 'b', 'c', 'd', 'e', 'f' ]
        db.library_path = lst
        self.assertEqual(len(db.library_path), len(lst))
        self.assertEqual(list(db.library_path), lst)
        del lst[3]
        del db.library_path[3]
        self.assertEqual(list(db.library_path), lst)
        del lst[5] # 'e'
        del db.library_path['e']
        self.assertEqual(list(db.library_path), lst)
        self.assertEqual(type(db.library_path), pypkgdepdb.StringListAccess)

    def elf_libfoo(self):
        elf = self.MakeElf('/usr/lib', 'libfoo.so',
                           rpath='/usr/lib:/usr/local/lib',
                           interp='/lib/ld-elf.so')
        elf.needed = ['libbar1.so', 'libbar2.so']
        return elf

    def elf_libbar1(self):
        return self.MakeElf('/usr/lib', 'libbar1.so',interp='/lib/ld-elf.so')

    def elf_libbar2(self):
        return self.MakeElf('/usr/lib', 'libbar2.so',interp='/lib/ld-elf.so')

    def pkg_libfoo(self):
        pkg = self.MakePkg('libfoo', '1.0-1', 'libfoo package',
            depends=['libc', 'libbar', ('libopt','>1')],
            makedepends=['check'],
            conflicts=['oldfoo'], replaces=['oldfoo'],
            provides=[('oldfoo','=1.0')],
            groups=['base', 'devel', 'foogroup'])
        pkg.filelist = ['usr/lib/libfoo.so',
                        'usr/lib/libfoo.so.1',
                        'usr/lib/libfoo.so.1.0']
        pkg.elfs.append(self.elf_libfoo())
        return pkg

    def pkg_libbar(self):
        pkg = self.MakePkg('libbar', '1.0-1', 'libbar package')
        pkg.filelist = ['usr/lib/libbar1.so',
                        'usr/lib/libbar1.so.1',
                        'usr/lib/libbar1.so.1.0',
                        'usr/lib/libbar2.so',
                        'usr/lib/libbar2.so.1',
                        'usr/lib/libbar2.so.1.0']
        pkg.elfs.append(self.elf_libbar1())
        pkg.elfs.append(self.elf_libbar2())
        return pkg

    def test_dbpkgs(self):
        db = pypkgdepdb.DB(self.cfg)
        db.name = 'A Database'
        db.library_path = ['/lib', '/usr/lib']
        self.assertEqual(len(db.library_path), 2)
        db.store('pa_db_test.db.gz')

        ck = pypkgdepdb.DB(self.cfg)
        ck.read('pa_db_test.db.gz')
        self.assertEqual(len(ck.library_path), 2)
        self.assertEqual(list(db.library_path), list(ck.library_path))
        del ck

        libfoo = self.pkg_libfoo()
        db.install(libfoo)
        self.assertEqual(len(db.packages), 1)
        self.assertTrue(db.is_broken(libfoo))

        libbar = self.pkg_libbar()
        db.install(libbar)
        self.assertEqual(len(db.packages), 2)
        self.assertFalse(db.is_broken(libfoo))

if __name__ == '__main__':
    unittest.main()
