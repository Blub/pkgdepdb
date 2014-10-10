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

    def MakePkg(self, name, version, description,
                depends=[], optdepends=[], makedepends=[], checkdepends=[],
                conflicts=[], replaces=[], provides=[], pkgbase=None):
        pkg = pypkgdepdb.Package()
        self.assertIsNotNone(pkg)
        pkg.name        = name
        pkg.version     = version
        pkg.description = description
        pkg.pkgbase     = pkgbase
        self.assertEqual(pkg.name        , name)
        self.assertEqual(pkg.version     , version)
        self.assertEqual(pkg.description , description)
        self.assertEqual(pkg.pkgbase     , pkgbase)

        pkg.depends.extend(depends)
        self.assertEqual(len(pkg.depends), len(depends))
        pkgdeps = pkg.depends
        for i in range(len(depends)):
            if type(depends[i]) == str:
                self.assertEqual(pkgdeps[i][0], depends[i])
                self.assertEqual(pkg.depends[i][0], depends[i])
            else:
                self.assertEqual(pkgdeps[i], depends[i])
                self.assertEqual(pkg.depends[i], depends[i])

        def filldeps(name, deps):
            getattr(pkg, name).extend(deps)
            pkgdeps = getattr(pkg, name)
            self.assertEqual(len(pkgdeps), len(deps))
            for i in range(len(deps)):
                if type(deps[i]) == str:
                    self.assertEqual(pkgdeps[i][0], deps[i])
                else:
                    self.assertEqual(pkgdeps[i], deps[i])
        filldeps('optdepends',   optdepends)
        filldeps('makedepends',  makedepends)
        filldeps('checkdepends', checkdepends)
        filldeps('conflicts',    conflicts)
        filldeps('replaces',     replaces)
        filldeps('provides',     provides)
        return pkg

    def test_pkg(self):
        foo = self.MakePkg('foo', '1.0-1', 'foo package',
                           pkgbase='foobase',
                           depends=[('libc', '>=1.0'), 'bar1'],
                           optdepends=['bar2'],
                           makedepends=[('gcc','>=4.9'), 'make'],
                           checkdepends=['check'],
                           conflicts=['oldfoo'],
                           replaces=['oldfoo'],
                           provides=[('oldfoo', '=1.0')])
        self.assertEqual(type(foo.makedepends[0]), tuple)

if __name__ == '__main__':
    unittest.main()
