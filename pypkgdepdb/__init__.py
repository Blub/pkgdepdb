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
    Depends     = 0
    OptDepends  = 1
    MakeDepends = 2
    Provides    = 3
    Conflicts   = 4
    Replaces    = 5

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
    ('db_wipe_file_lists',         c_int,    [p_db]),
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
    ('pkg_info_count_values',      c_size_t, [p_pkg, c_char_p]),
    ('pkg_info_get_values',        c_size_t, [p_pkg, c_char_p, POINTER(c_char_p), c_size_t, c_size_t]),
    ('pkg_info_add',               c_size_t, [p_pkg, c_char_p, c_char_p]),
    ('pkg_info_del_s',             c_size_t, [p_pkg, c_char_p, c_char_p]),
    ('pkg_info_del_i',             c_size_t, [p_pkg, c_char_p, c_size_t]),
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
            raise ToxException('failed to find function %s' % fn[0])
        func.restype  = fn[1]
        func.argtypes = fn[2]
        setattr(lib, fn[0], func)

loadfuncs(rawlib, lib, pkgdepdb_functions, 'pkgdepdb_')

def from_c_string2(addr, length):
    """utf-8 decode a C string into a python string"""
    return str(string_at(addr, length), encoding='utf-8')
def from_c_string(addr):
    """utf-8 decode a C string into a python string"""
    return str(string_at(addr), encoding='utf-8')

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

def StringProperty(c_getter, c_setter):
    def getter(self):
        return from_c_string(c_getter(self._ptr))
    def setter(self, value):
        return c_setter(self._ptr, value.encode('utf-8'))
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

    quiet              = BoolProperty(lib.cfg_quiet, lib.cfg_set_quiet)
    package_depends    = BoolProperty(lib.cfg_package_depends,
                                      lib.cfg_set_package_depends)
    package_file_lists = BoolProperty(lib.cfg_package_file_lists,
                                      lib.cfg_set_package_file_lists)
    package_info       = BoolProperty(lib.cfg_package_info,
                                      lib.cfg_set_package_info)

__all__ = [
            'PKGDepDBException',
            'p_cfg', 'p_db', 'p_pkg', 'p_elf',
            'LogLevel', 'PkgEntry', 'JSON',
            'rawlib',
            'lib',
            'Config', 'DB', 'Package', 'Pkg', 'Elf'
          ]
