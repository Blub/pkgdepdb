import ctypes

from . import functions
from .functions import p_cfg, p_db, p_pkg, p_elf
from .utils import *

SONAME = 'libpkgdepdb.so.1'

class PKGDepDBException(Exception):
    pass

class LogLevel(object):
    Debug   = 0
    Message = 1
    Print   = 2
    Warn    = 3
    Error   = 4

class PkgEntry(object):
    Depends      = 0
    OptDepends   = 1
    MakeDepends  = 2
    CheckDepends = 3
    Provides     = 4
    Conflicts    = 5
    Replaces     = 6

class JSON(object):
    Query = 1
    DB    = 2

class lib(object):
    pass

rawlib = ctypes.CDLL(SONAME, mode=ctypes.RTLD_GLOBAL)
if rawlib is None:
    raise PKGDepDBException('failed to open %s' % SONAME)

functions.load(rawlib, lib, functions.pkgdepdb_functions, 'pkgdepdb_')

class Config(object):
    def __init__(self):
        self._ptr = lib.cfg_new()
        if self._ptr is None:
            raise PKGDepDBException('failed to create config instance')

    def __del__(self):
        lib.cfg_delete(self._ptr)
        self._ptr = None

    def load(self, filepath):
        path = filepath.encode('utf-8')
        if lib.cfg_load(self._ptr, path) == 0:
            raise PKGDepDBException('failed to load from: %s' % (filepath))

    def load_default(self):
        if lib.cfg_load_default(self._ptr) == 0:
            raise PKGDepDBException('failed to load default config')

    database  = StringProperty(lib.cfg_database, lib.cfg_set_database)

    verbosity = IntProperty(lib.cfg_verbosity, lib.cfg_set_verbosity)
    log_level = IntProperty(lib.cfg_log_level, lib.cfg_set_log_level)
    json      = IntProperty(lib.cfg_json,      lib.cfg_set_json)

    quiet              = BoolProperty(lib.cfg_quiet, lib.cfg_set_quiet)
    package_depends    = BoolProperty(lib.cfg_package_depends,
                                      lib.cfg_set_package_depends)
    package_file_lists = BoolProperty(lib.cfg_package_file_lists,
                                      lib.cfg_set_package_file_lists)
    package_info       = BoolProperty(lib.cfg_package_info,
                                      lib.cfg_set_package_info)

class DB(object):
    class PackageList(object):
        def __init__(self, owner):
            self.owner = owner

        def __len__(self):
            return lib.db_package_count(self.owner._ptr)

        def get(self, off=0, count=None):
            if off < 0: raise IndexError
            maxcount = len(self)
            if off >= maxcount: raise IndexError
            count = count or maxcount
            if count < 0: raise ValueError('cannot fetch a negative count')
            count = min(count, maxcount - off)
            out = (p_pkg * count)()
            got = lib.db_package_get(self.owner._ptr, out, off, count)
            return [Package(x,True) for x in got]

        def __getitem__(self, key):
            if isinstance(key, slice):
                return self.__getslice__(self, key.start, key.stop, key.step)
            if isinstance(key, str):
                return lib.db_package_find(self.owner._ptr, cstr(key))
            return self.get(key, 1)[0]

        def __getslice__(self, start=None, stop=None, step=None):
            step  = step or 1
            start = start or 0
            count = stop - start if stop else None
            if step == 0: raise ValueError('step cannot be zero')
            if count == 0: return []
            if step > 0:
                if count < 0: return []
                return self.get(start, count)[::step]
            else:
                if count > 0: return []
                return self.get(start+count, -count)[-count:0:step]

        def __contains__(self, value):
            return value in self.get()

    def __init__(self, cfg):
        self._ptr = lib.db_new(cfg._ptr)
        if self._ptr is None:
            raise PKGDepDBException('failed to create database instance')
        self.library_path = StringListAccess(self,
                                             lib.db_library_path_count,
                                             lib.db_library_path_get,
                                             lib.db_library_path_add,
                                             lib.db_library_path_del_s,
                                             lib.db_library_path_del_i,
                                             lib.db_library_path_set_i)
        self.ignored_files = StringListAccess(self,
                                              lib.db_ignored_files_count,
                                              lib.db_ignored_files_get,
                                              lib.db_ignored_files_add,
                                              lib.db_ignored_files_del_s,
                                              lib.db_ignored_files_del_i)
        self.base_packages = StringListAccess(self,
                                              lib.db_base_packages_count,
                                              lib.db_base_packages_get,
                                              lib.db_base_packages_add,
                                              lib.db_base_packages_del_s,
                                              lib.db_base_packages_del_i)
        self.assume_found = StringListAccess(self,
                                             lib.db_assume_found_count,
                                             lib.db_assume_found_get,
                                             lib.db_assume_found_add,
                                             lib.db_assume_found_del_s,
                                             lib.db_assume_found_del_i)
        self.packages = DB.PackageList(self)

    loaded_version = IntGetter(lib.db_loaded_version)
    strict_linking = BoolProperty(lib.db_strict_linking,
                                  lib.db_set_strict_linking)
    name           = StringProperty(lib.db_name, lib.db_set_name)

    def __del__(self):
        lib.db_delete(self._ptr)

    def read(self, path):
        if lib.db_read(self._ptr, cstr(path)) != 1:
            raise PKGDepDBException('failed to read database from %s' % (path))

    def store(self, path):
        if lib.db_store(self._ptr, cstr(path)) != 1:
            raise PKGDepDBException('failed to store database to %s' % (path))

    def relink_all(self):
        lib.db_relink_all(self._ptr)

    def fix_paths(self):
        lib.db_fix_paths(self._ptr)

    def wipe_packages(self):
        return True if lib.db_wipe_packages(self._ptr) == 1 else False
    def wipe_filelists(self):
        return True if lib.db_wipe_file_lists(self._ptr) == 1 else False

    def install(self, pkg):
        if lib.db_package_install(self._ptr, pkg._ptr) != 1:
            raise PKGDepDBException('package installation failed')
        pkg.linked = True

    def uninstall_package(self, pkg):
        if lib.db_package_remove(self._ptr, pkg._ptr) != 1:
            raise PKGDepDBException('failed to remove package')
        pkg.linked = False

    def delete_package(self, pkg):
        if lib.db_package_delete_p(self._ptr, pkg._ptr) != 1:
            raise PKGDepDBException('failed to uninstall package')
        pkg.linked = False
        del pkg

    def is_broken(self, what):
        if type(what) == Package:
            v = lib.db_package_is_broken(self._ptr, what._ptr)
        elif type(what) == Elf:
            v = lib.db_object_is_broken(self._ptr, what._ptr)
        else:
            raise TypeError('object must be a Package or Elf instance')
        return True if v == 1 else False

class Package(object):
    def __init__(self, ptr=None, linked=False):
        self._ptr = ptr or lib.pkg_new()
        if self._ptr is None:
            raise PKGDepDBException('failed to create package instance')
        self.linked = linked

        self.groups = StringListAccess(self,
                                       lib.pkg_groups_count,
                                       lib.pkg_groups_get,
                                       lib.pkg_groups_add,
                                       lib.pkg_groups_del_s,
                                       lib.pkg_groups_del_i)
        self.filelist = StringListAccess(self,
                                         lib.pkg_filelist_count,
                                         lib.pkg_filelist_get,
                                         lib.pkg_filelist_add,
                                         lib.pkg_filelist_del_s,
                                         lib.pkg_filelist_del_i)
        self.info = StringMapOfStringList(self,
                                          lib.pkg_info_count_keys,
                                          lib.pkg_info_get_keys,
                                          lib.pkg_info_count_values,
                                          lib.pkg_info_get_values,
                                          lib.pkg_info_add,
                                          lib.pkg_info_del_s,
                                          lib.pkg_info_del_i,
                                          lib.pkg_info_set_i,
                                          lib.pkg_info_contains_key)
        def make_deplist(self, what):
            return DepListAccess(self,
                lambda o:       lib.pkg_dep_count   (o, what),
                lambda o, *arg: lib.pkg_dep_get     (o, what, *arg),
                lambda o, *arg: lib.pkg_dep_add     (o, what, *arg),
                lambda o, *arg: lib.pkg_dep_del_name(o, what, *arg),
                lambda o, *arg: lib.pkg_dep_del_full(o, what, *arg),
                lambda o, *arg: lib.pkg_dep_del_i   (o, what, *arg),
                lambda o, *arg: lib.pkg_dep_set_i   (o, what, *arg))
        self.depends      = make_deplist(self, PkgEntry.Depends)
        self.optdepends   = make_deplist(self, PkgEntry.OptDepends)
        self.makedepends  = make_deplist(self, PkgEntry.MakeDepends)
        self.checkdepends = make_deplist(self, PkgEntry.CheckDepends)
        self.provides     = make_deplist(self, PkgEntry.Provides)
        self.conflicts    = make_deplist(self, PkgEntry.Conflicts)
        self.replaces     = make_deplist(self, PkgEntry.Replaces)

    name        = StringProperty(lib.pkg_name, lib.pkg_set_name)
    version     = StringProperty(lib.pkg_version, lib.pkg_set_version)
    pkgbase     = StringProperty(lib.pkg_pkgbase, lib.pkg_set_pkgbase)
    description = StringProperty(lib.pkg_description, lib.pkg_set_description)

    def __del__(self):
        if not self.linked:
            lib.pkg_delete(self._ptr)

    @staticmethod
    def load(path, cfg):
        ptr = lib.pkg_load(cstr(path), cfg._ptr)
        if ptr is None:
            raise PKGDepDBException('failed to load package from: %s' % (path))
        return Package(ptr, False)

    def read_info(self, pkginfo_text, cfg):
        if lib.pkg_read_info(self._ptr, cstr(pkginfo_text), len(pkginfo_text),
                             cfg._ptr) != 1:
            raise PKGDepDBException('error parsing PKGINFO')

    def guess_version(self, filename):
        lib.pkg_guess_version(self._ptr, cstr(filename))

    def conflicts(self, other):
        if type(other) != Package:
            raise TypeError('other must be a package')
        return True if lib.pkg_conflict(self._ptr, other._ptr) else False

    def replaces(self, other):
        if type(other) != Package:
            raise TypeError('other must be a package')
        return True if lib.pkg_replaces(self._ptr, other._ptr) else False

__all__ = [
            'PKGDepDBException',
            'p_cfg', 'p_db', 'p_pkg', 'p_elf',
            'LogLevel', 'PkgEntry', 'JSON',
            'rawlib',
            'lib',
            'Config', 'DB', 'Package', 'Pkg', 'Elf'
          ]
