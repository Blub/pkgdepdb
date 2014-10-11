import unittest
import ctypes

import pypkgdepdb

class TestConfig(unittest.TestCase):
    def setUp(self):
        self.cfg = pypkgdepdb.Config()
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

if __name__ == '__main__':
    unittest.main()
