from ctypes import *

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

p_cfg = POINTER(c_void_p)
p_db  = POINTER(c_void_p)
p_pkg = POINTER(c_void_p)
p_elf = POINTER(c_void_p)

pkgdepdb_functions = [
    ('cfg_new',                    p_cfg,    []),
    ('cfg_delete',                 None,     [p_cfg]),
    ('cfg_load',                   c_int,    [p_cfg, c_char_p]),
    ('cfg_load_default',           c_int,    [p_cfg]),
    ('cfg_database',               c_char_p, [p_cfg]),
    ('cfg_set_database',           None,     [p_cfg, c_char_p]),
    ('cfg_verbosity',              c_uint,   [p_cfg]),
    ('cfg_set_verbosity',          None,     [p_cfg, c_uint]),
    ('cfg_quiet',                  c_int,    [p_cfg]),
    ('cfg_set_quiet',              None,     [p_cfg, c_int]),
    ('cfg_package_depends',        c_int,    [p_cfg]),
    ('cfg_set_package_depends',    None,     [p_cfg, c_int]),
    ('cfg_package_file_lists',     c_int,    [p_cfg]),
    ('cfg_set_package_file_lists', None,     [p_cfg, c_int]),
    ('cfg_package_info',           c_int,    [p_cfg]),
    ('cfg_set_package_info',       None,     [p_cfg, c_int]),
    ('cfg_max_jobs',               c_uint,   [p_cfg]),
    ('cfg_set_max_jobs',           None,     [p_cfg, c_uint]),
    ('cfg_log_level',              c_uint,   [p_cfg]),
    ('cfg_set_log_level',          None,     [p_cfg, c_uint]),
    ('cfg_json',                   c_uint,   [p_cfg]),
    ('cfg_set_json',               None,     [p_cfg, c_uint]),
    ('db_new',                     p_db,     [p_cfg]),
    ('db_delete',                  None,     [p_db]),
    ('db_read',                    c_int,    [p_db, c_char_p]),
    ('db_store',                   c_int,    [p_db, c_char_p]),
    ('db_loaded_version',          c_uint,   [p_db]),
    ('db_strict_linking',          c_int,    [p_db]),
    ('db_set_strict_linking',      None,     [p_db, c_int]),
    ('db_name',                    c_char_p, [p_db]),
    ('db_set_name',                None,     [p_db, c_char_p]),
    ('db_library_path_count',      c_size_t, [p_db]),
    ('db_library_path_get',        c_size_t, [p_db, POINTER(c_char_p), c_size_t, c_size_t]),
    ('db_library_path_add',        c_int,    [p_db, c_char_p]),
    ('db_library_path_del_s',      c_int,    [p_db, c_char_p]),
    ('db_library_path_del_i',      c_int,    [p_db, c_size_t]),
    ('db_library_path_set_i',      c_int,    [p_db, c_size_t, c_char_p]),
    ('db_package_count',           c_size_t, [p_db]),
    ('db_package_install',         c_size_t, [p_db, p_pkg]),
    ('db_package_delete_p',        c_size_t, [p_db, p_pkg]),
    ('db_package_delete_s',        c_size_t, [p_db, c_char_p]),
    ('db_package_delete_i',        c_size_t, [p_db, c_size_t]),
    ('db_package_remove',          c_size_t, [p_db, p_pkg]),
    ('db_package_is_broken',       c_int,    [p_db, p_pkg]),
    ('db_object_count',            c_size_t, [p_db]),
    ('db_object_get',              c_size_t, [p_db, POINTER(p_elf), c_size_t, c_size_t]),
    ('db_object_is_broken',        c_int,    [p_db, p_elf]),
    ('db_ignored_files_count',     c_size_t, [p_db]),
    ('db_ignored_files_get',       c_size_t, [p_db, POINTER(c_char_p), c_size_t, c_size_t]),
    ('db_ignored_files_add',       c_int,    [p_db, c_char_p]),
    ('db_ignored_files_del_s',     c_int,    [p_db, c_char_p]),
    ('db_ignored_files_del_i',     c_int,    [p_db, c_size_t]),
    ('db_base_packages_count',     c_size_t, [p_db]),
    ('db_base_packages_get',       c_size_t, [p_db, POINTER(c_char_p), c_size_t, c_size_t]),
    ('db_base_packages_add',       c_size_t, [p_db, c_char_p]),
    ('db_base_packages_del_s',     c_int,    [p_db, c_char_p]),
    ('db_base_packages_del_i',     c_int,    [p_db, c_size_t]),
    ('db_assume_found_count',      c_size_t, [p_db]),
    ('db_assume_found_get',        c_size_t, [p_db, POINTER(c_char_p), c_size_t, c_size_t]),
    ('db_assume_found_add',        c_int,    [p_db, c_char_p]),
    ('db_assume_found_del_s',      c_int,    [p_db, c_char_p]),
    ('db_assume_found_del_i',      c_int,    [p_db, c_size_t]),
    ('db_relink_all',              None,     [p_db]),
    ('db_fix_paths',               None,     [p_db]),
    ('db_wipe_packages',           c_int,    [p_db]),
    ('db_wipe_filelists',          c_int,    [p_db]),
    ('pkg_new',                    p_pkg,    []),
    ('pkg_load',                   p_pkg,    [c_char_p, p_cfg]),
    ('pkg_delete',                 None,     [p_pkg]),
    ('pkg_name',                   c_char_p, [p_pkg]),
    ('pkg_version',                c_char_p, [p_pkg]),
    ('pkg_pkgbase',                c_char_p, [p_pkg]),
    ('pkg_description',            c_char_p, [p_pkg]),
    ('pkg_set_name',               None,     [p_pkg, c_char_p]),
    ('pkg_set_version',            None,     [p_pkg, c_char_p]),
    ('pkg_set_pkgbase',            None,     [p_pkg, c_char_p]),
    ('pkg_set_description',        None,     [p_pkg, c_char_p]),
    ('pkg_read_info',              c_int,    [p_pkg, c_char_p, c_size_t, p_cfg]),
    ('pkg_dep_count',              c_size_t, [p_pkg, c_uint]),
    ('pkg_dep_get',                c_size_t, [p_pkg, c_uint, POINTER(c_char_p), POINTER(c_char_p), c_size_t, c_size_t]),
    ('pkg_dep_add',                c_int,    [p_pkg, c_uint, c_char_p, c_char_p]),
    ('pkg_dep_del_name',           c_int,    [p_pkg, c_uint, c_char_p]),
    ('pkg_dep_del_full',           c_int,    [p_pkg, c_uint, c_char_p, c_char_p]),
    ('pkg_dep_del_i',              c_int,    [p_pkg, c_uint, c_size_t]),
    ('pkg_groups_count',           c_size_t, [p_pkg]),
    ('pkg_groups_get',             c_size_t, [p_pkg, POINTER(c_char_p), c_size_t, c_size_t]),
    ('pkg_groups_add',             c_size_t, [p_pkg, c_char_p]),
    ('pkg_groups_del_s',           c_size_t, [p_pkg, c_char_p]),
    ('pkg_groups_del_i',           c_size_t, [p_pkg, c_size_t]),
    ('pkg_filelist_count',         c_size_t, [p_pkg]),
    ('pkg_filelist_get',           c_size_t, [p_pkg, POINTER(c_char_p), c_size_t, c_size_t]),
    ('pkg_filelist_add',           c_size_t, [p_pkg, c_char_p]),
    ('pkg_filelist_del_s',         c_size_t, [p_pkg, c_char_p]),
    ('pkg_filelist_del_i',         c_size_t, [p_pkg, c_size_t]),
    ('pkg_info_count_keys',        c_size_t, [p_pkg]),
    ('pkg_info_get_keys',          c_size_t, [p_pkg, POINTER(c_char_p), c_size_t, c_size_t]),
    ('pkg_info_contains_key',      c_size_t, [p_pkg, c_char_p]),
    ('pkg_info_count_values',      c_size_t, [p_pkg, c_char_p]),
    ('pkg_info_get_values',        c_size_t, [p_pkg, c_char_p, POINTER(c_char_p), c_size_t, c_size_t]),
    ('pkg_info_add',               c_size_t, [p_pkg, c_char_p, c_char_p]),
    ('pkg_info_del_s',             c_size_t, [p_pkg, c_char_p, c_char_p]),
    ('pkg_info_del_i',             c_size_t, [p_pkg, c_char_p, c_size_t]),
    ('pkg_info_set_i',             c_size_t, [p_pkg, c_char_p, c_size_t, c_char_p]),
    ('pkg_elf_count',              c_size_t, [p_pkg]),
    ('pkg_elf_get',                c_size_t, [p_pkg, POINTER(p_elf), c_size_t, c_size_t]),
    ('pkg_elf_add',                c_size_t, [p_pkg, p_elf]),
    ('pkg_elf_del_e',              c_size_t, [p_pkg, p_elf]),
    ('pkg_elf_del_i',              c_size_t, [p_pkg, c_size_t]),
    ('pkg_guess_version',          None,     [p_pkg, c_char_p]),
    ('pkg_conflict',               c_int,    [p_pkg, p_pkg]),
    ('pkg_replaces',               c_int,    [p_pkg, p_pkg]),
    ('elf_new',                    p_elf,    []),
    ('elf_unref',                  None,     [p_elf]),
    ('elf_open',                   p_elf,    [c_char_p, POINTER(c_int), p_cfg]),
    ('elf_read',                   p_elf,    [c_char_p, c_size_t, c_char_p, c_char_p, POINTER(c_int), p_cfg]),
    ('elf_dirname',                c_char_p, [p_elf]),
    ('elf_basename',               c_char_p, [p_elf]),
    ('elf_set_dirname',            None,     [p_elf, c_char_p]),
    ('elf_set_basename',           None,     [p_elf, c_char_p]),
    ('elf_class',                  c_ubyte,  [p_elf]),
    ('elf_data',                   c_ubyte,  [p_elf]),
    ('elf_osabi',                  c_ubyte,  [p_elf]),
    ('elf_set_class',              None,     [p_elf, c_ubyte]),
    ('elf_set_data',               None,     [p_elf, c_ubyte]),
    ('elf_set_osabi',              None,     [p_elf, c_ubyte]),
    ('elf_class_string',           c_char_p, [p_elf]),
    ('elf_data_string',            c_char_p, [p_elf]),
    ('elf_osabi_string',           c_char_p, [p_elf]),
    ('elf_rpath',                  c_char_p, [p_elf]),
    ('elf_runpath',                c_char_p, [p_elf]),
    ('elf_interpreter',            c_char_p, [p_elf]),
    ('elf_set_rpath',              None,     [p_elf, c_char_p]),
    ('elf_set_runpath',            None,     [p_elf, c_char_p]),
    ('elf_set_interpreter',        None,     [p_elf, c_char_p]),
    ('elf_needed_count',           c_size_t, [p_elf]),
    ('elf_needed_get',             c_size_t, [p_elf, POINTER(c_char_p), c_size_t, c_size_t]),
    ('elf_needed_contains',        c_int,    [p_elf, c_char_p]),
    ('elf_needed_add',             None,     [p_elf, c_char_p]),
    ('elf_needed_del_s',           c_int,    [p_elf, c_char_p]),
    ('elf_needed_del_i',           None,     [p_elf, c_size_t]),
    ('elf_can_use',                c_int,    [p_elf, p_elf, c_int]),
]

rawlib = CDLL(SONAME, mode=RTLD_GLOBAL)
if rawlib is None:
    raise PKGDepDBException('failed to open %s' % SONAME)

class lib(object):
    pass

def loadfuncs(rawlib, lib, funcs, allprefix):
    for fn in funcs:
        func = getattr(rawlib, allprefix + fn[0], None)
        if func is None:
            raise PKGDepDBException('failed to find function %s' % fn[0])
        func.restype  = fn[1]
        func.argtypes = fn[2]
        setattr(lib, fn[0], func)

loadfuncs(rawlib, lib, pkgdepdb_functions, 'pkgdepdb_')

def from_c_string2(addr, length):
    """utf-8 decode a C string into a python string"""
    return str(string_at(addr, length), encoding='utf-8')
def from_c_string(addr):
    """utf-8 decode a C string into a python string"""
    return str(addr, encoding='utf-8')
def cstr(s):
    """convenience function to encode a str as bytes"""
    return s.encode('utf-8')

def BoolProperty(c_getter, c_setter):
    def getter(self):
        return True if c_getter(self._ptr) != 0 else False
    def setter(self, value):
        return c_setter(self._ptr, value)
    return property(getter, setter)

def IntProperty(c_getter, c_setter):
    def getter(self):
        return int(c_getter(self._ptr))
    def setter(self, value):
        return c_setter(self._ptr, value)
    return property(getter, setter)

def IntGetter(getter):
    def getter(self):
        return int(getter(self._ptr))
    return property(getter)

def StringProperty(c_getter, c_setter):
    def getter(self):
        return from_c_string(c_getter(self._ptr))
    def setter(self, value):
        return c_setter(self._ptr, cstr(value))
    return property(getter, setter)

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

class StringListAccess(object):
    def __init__(self, owner, count, get, add, del_s, del_i, set_i=None):
        self.owner    = owner
        self.__count  = count
        self.__get    = get
        self.__add    = add
        self.__del_s  = del_s
        self.__del_i  = del_i
        if set_i is None:
            self.__setitem__ = None
        else:
            self.__set_i  = set_i

    def __len__(self):
        return self.__count(self.owner._ptr)

    def __getitem__(self, key):
        if isinstance(key, slice):
            return self.__getslice__(self, key.start, key.stop, key.step)
        if not isinstance(key, int): raise TypeError
        if key < 0: raise IndexError
        count = self.__len__()
        if key >= count: raise IndexError
        return self.get(key, 1)[0]

    def __getslice__(self, start=None, stop=None, step=None):
        step  = step or 1
        start = start or 0
        count = stop - start if stop else None
        if step == 0:
            raise ValueError('step cannot be zero')
        if count == 0:
            return []
        if step > 0:
            if count < 0:
                return []
            values = self.get(start, count)
            return values[::step]
        else:
            if count > 0:
                return []
            values = self.get(start+count, -count)
            return values[start:start+count:step]

    def __setitem__(self, key, value):
        if isinstance(key, slice):
            return self.__setslice__(key.start, key.stop, value, key.step)
        if not isinstance(key, int): raise TypeError
        if key < 0: raise IndexError
        count = self.__len__()
        if key > count:
            raise IndexError
        elif key == count:
            return self.__add(self.owner._ptr, cstr(value))
        else:
            return self.__set_i(self.owner._ptr, key, cstr(value))

    def __setslice__(self, start, stop, values, step=None):
        count = len(self)
        start = start or 0
        stop  = stop  or count
        step  = step  or 1
        if step == 0:
            raise ValueError('step cannot be zero')
        if step > 0:
            if start < 0:
                raise IndexError
            for v in values:
                if start >= stop:
                    return
                if start == count:
                    if self.__add(self.owner._ptr, cstr(v)) == 1:
                        count += 1
                elif start > count:
                    raise IndexError
                else:
                    self.__set_i(self.owner._ptr, start, cstr(v))
                start += step
        else:
            for v in values:
                if start <= stop:
                    return
                if start < 0:
                    raise IndexError
                if start == count:
                    if self.__add(self.owner._ptr, cstr(v)) == 1:
                        count += 1
                elif start > count:
                    raise IndexError
                else:
                    self.__set_i(self.owner._ptr, start, cstr(v))
                start += step

    def __delitem__(self, key):
        if isinstance(key, slice):
            return self.__delslice__(self, key.start, key.stop, key.step)
        if isinstance(key, int):
            if self.__del_i(self.owner._ptr, int(key)) != 1:
                raise IndexError
            return
        if self.__del_s(self.owner._ptr, cstr(key)) != 1:
            raise IndexError

    def __delslice__(self, start=None, stop=None, step=None):
        step  = step or 1
        start = start or 0
        stop  = stop   or self.count()
        if step == 0:
            raise ValueError('step cannot be zero')
        if step > 0:
            s = slice(start, stop, step)
        else:
            s = reverse(slice(start, stop, step))
        for idx in s:
            print(idx)

    def __contains__(self, value):
        return value in self.get()

    class Iterator(object):
        def __init__(self, owner, beg, end):
            self.owner = owner
            self.index = beg
            self.count = end
        def __iter__(self):
            return self
        def next(self):
            if self.index >= self.count:
                raise StopIteration
            v = self.owner[self.index]
            self.index += 1
            return v
        def __next__(self):
            return self.next()
    def __iter__(self):
        return self.Iterator(self, 0, len(self))
    def __reversed__(self):
        return self.Iterator(self, len(self)-1, -1)

    def get(self, offset=None, count=None):
        avail = self.__count(self.owner._ptr)
        if offset is None:
            offset = 0
            if count is None:
                count = avail
        elif count is None:
            count = 1
        if count > avail - offset:
            count = avail - offset
        lst = (c_char_p * count)()
        got = self.__get(self.owner._ptr, lst, offset, count)
        return [ from_c_string(x) for x in lst[0:got] ]

    def add(self, s):
        return True if self.__add(self.owner._ptr, cstr(s)) else False

    def delete(self, what):
        if isinstance(what, str):
            return True if self.__del_s(self.owner._ptr, cstr(s)) else False
        elif isinstance(what, int):
            return True if sefl.__del_i(self.owner._ptr, s) else False
        else:
            raise PKGDepDBException(
                'invalid type for delete operation: %s (%s)' %
                (type(what), str(what)))

    def append(self, value):
        return self.add(value)
    def extend(self, value):
        for i in value:
            self.append(value)

class StringMapOfStringList(object):
    def __init__(self, owner, count_keys, get_keys, count_values, get_values,
                 add, del_kv, del_ki, set_ki):
        self.owner    = owner
        self.__count_keys   = count_keys
        self.__get_keys     = get_keys
        self.__count_values = count_values
        self.__get_values   = get_values
        self.__add    = add
        self.__del_kv  = del_kv
        self.__del_ki  = del_ki
        self.__set_ki  = set_ki

    def __len__(self):
        return self.__count_keys(self.owner._ptr)

    def __getitem__(self, key):
        if not isinstance(key, str): raise TypeError
        ckey = cstr(key)
        #if lib.pkg_info_contains_key(self.owner._ptr, ckey) != 1:
        #    raise KeyError('no such key: %s' % (key))
        return StringListAccess(self.owner,
            lambda owner: self.__count_values(owner, ckey),
            lambda owner, d, o, cnt: self.__get_values(owner, ckey, d, o, cnt),
            lambda owner, v: self.__add(owner, ckey, v),
            lambda owner, s: self.__del_kv(owner, ckey, s),
            lambda owner, i: self.__del_ki(owner, ckey, i),
            lambda owner, i, v: self.__set_ki(owner, ckey, i, v))

    def __delitem__(self, key):
        if not isinstance(key, str): raise TypeError
        ckey = cstr(key)
        while lib.pkg_info_del_i(self._ptr, ckey, 0) == 1: pass

    def __contains__(self, key):
        return (True if lib.pkg_info_contains_key(self.owner._ptr, cstr(key))
                else False)

    def __iter__(self):
        def iter():
            for key in self.get_keys():
                yield key
        return iter
    def __reversed__(self):
        def iter():
            for key in reverse(self.get_keys()):
                yield key
        return iter

    def get_keys(self, offset=None, count=None):
        avail = self.__count_keys(self.owner._ptr)
        if offset is None:
            offset = 0
            if count is None:
                count = avail
        elif count is None:
            count = 1
        if count > avail - offset:
            count = avail - offset
        lst = (c_char_p * count)()
        got = self.__get_keys(self.owner._ptr, lst, offset, count)
        return [ from_c_string(x) for x in lst[0:got] ]

class DB(object):
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

class Package(object):
    def __init__(self, ptr=None):
        self._ptr = ptr or lib.pkg_new()
        if self._ptr is None:
            raise PKGDepDBException('failed to create package instance')

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
                                          lib.pkg_info_set_i)

    name        = StringProperty(lib.pkg_name, lib.pkg_set_name)
    version     = StringProperty(lib.pkg_version, lib.pkg_set_version)
    pkgbase     = StringProperty(lib.pkg_pkgbase, lib.pkg_set_pkgbase)
    description = StringProperty(lib.pkg_description, lib.pkg_set_description)

    def __del__(self):
        lib.pkg_delete(self._ptr)

    @staticmethod
    def load(path, cfg):
        ptr = lib.pkg_load(cstr(path), cfg._ptr)
        if ptr is None:
            raise PKGDepDBException('failed to load package from: %s' % (path))
        return Package(ptr)

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
