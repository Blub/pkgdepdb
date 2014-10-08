#ifndef PKGDEPDB_EXT_H__
#define PKGDEPDB_EXT_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef int pkgdepdb_bool;

/* ELF objects are reference counted pointers */
typedef struct pkgdepdb_elf_* pkgdepdb_elf;
typedef struct pkgdepdb_pkg_  pkgdepdb_pkg;
typedef struct pkgdepdb_db_   pkgdepdb_db;
typedef struct pkgdepdb_cfg_  pkgdepdb_cfg;

/* core library interface */
void          pkgdepdb_init(void);
const char*   pkgdepdb_error(void);
void          pkgdepdb_set_error(const char*, ...);
void          pkgdepdb_clear_error(void);
void          pkgdepdb_finalize(void);

/*********
 * pkgdepdb::Config interface
 */
pkgdepdb_cfg*    pkgdepdb_cfg_new   (void);
void             pkgdepdb_cfg_delete(pkgdepdb_cfg*);

pkgdepdb_bool    pkgdepdb_cfg_load(pkgdepdb_cfg*, const char *filepath);
pkgdepdb_bool    pkgdepdb_cfg_load_default(pkgdepdb_cfg*);

const char*      pkgdepdb_cfg_database     (pkgdepdb_cfg*);
void             pkgdepdb_cfg_set_database (pkgdepdb_cfg*, const char*);

unsigned int     pkgdepdb_cfg_verbosity    (pkgdepdb_cfg*);
void             pkgdepdb_cfg_set_verbosity(pkgdepdb_cfg*, unsigned int);

/* some boolean properties */
pkgdepdb_bool    pkgdepdb_cfg_quiet                 (pkgdepdb_cfg*);
void             pkgdepdb_cfg_set_quiet             (pkgdepdb_cfg*, pkgdepdb_bool);
pkgdepdb_bool    pkgdepdb_cfg_package_depends       (pkgdepdb_cfg*);
void             pkgdepdb_cfg_set_package_depends   (pkgdepdb_cfg*, pkgdepdb_bool);
pkgdepdb_bool    pkgdepdb_cfg_package_file_lists    (pkgdepdb_cfg*);
void             pkgdepdb_cfg_set_package_file_lists(pkgdepdb_cfg*, pkgdepdb_bool);
pkgdepdb_bool    pkgdepdb_cfg_package_info          (pkgdepdb_cfg*);
void             pkgdepdb_cfg_set_package_info      (pkgdepdb_cfg*, pkgdepdb_bool);

/* threading */
unsigned int     pkgdepdb_cfg_max_jobs    (pkgdepdb_cfg*);
void             pkgdepdb_cfg_set_max_jobs(pkgdepdb_cfg*, unsigned int);

/* output */
enum {
  PKGDEPDB_CFG_LOG_LEVEL_DEBUG,
  PKGDEPDB_CFG_LOG_LEVEL_MESSAGE,
  PKGDEPDB_CFG_LOG_LEVEL_PRINT,
  PKGDEPDB_CFG_LOG_LEVEL_WARN,
  PKGDEPDB_CFG_LOG_LEVEL_ERROR,
};
unsigned int     pkgdepdb_cfg_log_level    (pkgdepdb_cfg*);
void             pkgdepdb_cfg_set_log_level(pkgdepdb_cfg*, unsigned int);

/* json - this part of the interface should not usually be required... */
#define PKGDEPDB_JSONBITS_QUERY (1<<0)
#define PKGDEPDB_JSONBITS_DB    (1<<1)
unsigned int     pkgdepdb_cfg_json    (pkgdepdb_cfg*);
void             pkgdepdb_cfg_set_json(pkgdepdb_cfg*, unsigned int);

/*********
 * pkgdepdb::DB interface
 */

pkgdepdb_db  *pkgdepdb_db_new   (pkgdepdb_cfg*);
void          pkgdepdb_db_delete(pkgdepdb_db*);
pkgdepdb_bool pkgdepdb_db_read  (pkgdepdb_db*, const char *filename);
pkgdepdb_bool pkgdepdb_db_store (pkgdepdb_db*, const char *filename);

unsigned int  pkgdepdb_db_loaded_version(pkgdepdb_db*);

pkgdepdb_bool pkgdepdb_db_strict_linking(pkgdepdb_db*);
void          pkgdepdb_db_set_strict_linking(pkgdepdb_db*, pkgdepdb_bool);

const char*   pkgdepdb_db_name(pkgdepdb_db*);
void          pkgdepdb_db_set_name(pkgdepdb_db*, const char*);

size_t        pkgdepdb_db_library_path_count(pkgdepdb_db*);
size_t        pkgdepdb_db_library_path_get  (pkgdepdb_db*, const char**, size_t,
                                             size_t);
pkgdepdb_bool pkgdepdb_db_library_path_add  (pkgdepdb_db*, const char*);
pkgdepdb_bool pkgdepdb_db_library_path_del_s(pkgdepdb_db*, const char*);
pkgdepdb_bool pkgdepdb_db_library_path_del_i(pkgdepdb_db*, size_t);

/* the package delete functions "uninstall" the package from the db */
size_t pkgdepdb_db_package_count   (pkgdepdb_db*);
size_t pkgdepdb_db_package_install (pkgdepdb_db*, pkgdepdb_pkg*);
size_t pkgdepdb_db_package_delete_p(pkgdepdb_db*, pkgdepdb_pkg*);
size_t pkgdepdb_db_package_delete_s(pkgdepdb_db*, const char*);
size_t pkgdepdb_db_package_delete_i(pkgdepdb_db*, size_t);

/* remove a package without destroying it */
size_t pkgdepdb_db_package_remove(pkgdepdb_db*, pkgdepdb_pkg*);

pkgdepdb_bool pkgdepdb_db_package_is_broken(pkgdepdb_db*, pkgdepdb_pkg*);

/* the DB's object list is read-only */
size_t pkgdepdb_db_object_count(pkgdepdb_db*);
size_t pkgdepdb_db_object_get  (pkgdepdb_db*, pkgdepdb_elf*, size_t, size_t);

pkgdepdb_bool pkgdepdb_db_object_is_broken(pkgdepdb_db*, pkgdepdb_elf);

size_t        pkgdepdb_db_ignored_files_count(pkgdepdb_db*);
size_t        pkgdepdb_db_ignored_files_get  (pkgdepdb_db*, const char**,
                                              size_t, size_t);
pkgdepdb_bool pkgdepdb_db_ignored_files_add  (pkgdepdb_db*, const char*);
pkgdepdb_bool pkgdepdb_db_ignored_files_del_s(pkgdepdb_db*, const char*);
pkgdepdb_bool pkgdepdb_db_ignored_files_del_i(pkgdepdb_db*, size_t);

size_t        pkgdepdb_db_base_packages_count(pkgdepdb_db*);
size_t        pkgdepdb_db_base_packages_get  (pkgdepdb_db*, const char**,
                                              size_t, size_t);
size_t        pkgdepdb_db_base_packages_add  (pkgdepdb_db*, const char*);
pkgdepdb_bool pkgdepdb_db_base_packages_del_s(pkgdepdb_db*, const char*);
pkgdepdb_bool pkgdepdb_db_base_packages_del_i(pkgdepdb_db*, size_t);

size_t        pkgdepdb_db_assume_found_count(pkgdepdb_db*);
size_t        pkgdepdb_db_assume_found_get  (pkgdepdb_db*, const char**,
                                              size_t, size_t);
pkgdepdb_bool pkgdepdb_db_assume_found_add  (pkgdepdb_db*, const char*);
pkgdepdb_bool pkgdepdb_db_assume_found_del_s(pkgdepdb_db*, const char*);
pkgdepdb_bool pkgdepdb_db_assume_found_del_i(pkgdepdb_db*, size_t);

void          pkgdepdb_db_relink_all     (pkgdepdb_db*);
void          pkgdepdb_db_fix_paths      (pkgdepdb_db*);
pkgdepdb_bool pkgdepdb_db_wipe_packages  (pkgdepdb_db*);
pkgdepdb_bool pkgdepdb_db_wipe_file_lists(pkgdepdb_db*);

/*********
 * pkgdepdb::Package interface
 */

pkgdepdb_pkg* pkgdepdb_pkg_new   (void);
pkgdepdb_pkg* pkgdepdb_pkg_load  (const char *filename, pkgdepdb_cfg*);
void          pkgdepdb_pkg_delete(pkgdepdb_pkg*);

const char*   pkgdepdb_pkg_name       (pkgdepdb_pkg*);
const char*   pkgdepdb_pkg_version    (pkgdepdb_pkg*);
const char*   pkgdepdb_pkg_pkgbase    (pkgdepdb_pkg*);
const char*   pkgdepdb_pkg_description(pkgdepdb_pkg*);
void          pkgdepdb_pkg_set_name       (pkgdepdb_pkg*, const char*);
void          pkgdepdb_pkg_set_version    (pkgdepdb_pkg*, const char*);
void          pkgdepdb_pkg_set_pkgbase    (pkgdepdb_pkg*, const char*);
void          pkgdepdb_pkg_set_description(pkgdepdb_pkg*, const char*);

pkgdepdb_bool pkgdepdb_pkg_read_info(pkgdepdb_pkg*, const char*, size_t,
                                     pkgdepdb_cfg*);

enum {
  PKGDEPDB_PKG_DEPENDS,
  PKGDEPDB_PKG_OPTDEPENDS,
  PKGDEPDB_PKG_MAKEDEPENDS,
  PKGDEPDB_PKG_PROVIDES,
  PKGDEPDB_PKG_CONFLICTS,
  PKGDEPDB_PKG_REPLACES,
};
size_t        pkgdepdb_pkg_dep_count   (pkgdepdb_pkg*, unsigned int what);
size_t        pkgdepdb_pkg_dep_get     (pkgdepdb_pkg*, unsigned int what,
                                        const char**, const char**, size_t,
                                        size_t);
size_t        pkgdepdb_pkg_dep_add     (pkgdepdb_pkg*, unsigned int what,
                                        const char*, const char*);
size_t        pkgdepdb_pkg_dep_del_name(pkgdepdb_pkg*, unsigned int what,
                                        const char*);
size_t        pkgdepdb_pkg_dep_del_full(pkgdepdb_pkg*, unsigned int what,
                                        const char*, const char*);
size_t        pkgdepdb_pkg_dep_del_i   (pkgdepdb_pkg*, unsigned int what,
                                        size_t);

size_t        pkgdepdb_pkg_groups_count(pkgdepdb_pkg*);
size_t        pkgdepdb_pkg_groups_get  (pkgdepdb_pkg*, const char**, size_t,
                                        size_t);
size_t        pkgdepdb_pkg_groups_add  (pkgdepdb_pkg*, const char*);
size_t        pkgdepdb_pkg_groups_del_s(pkgdepdb_pkg*, const char*);
size_t        pkgdepdb_pkg_groups_del_i(pkgdepdb_pkg*, size_t);

size_t        pkgdepdb_pkg_filelist_count(pkgdepdb_pkg*);
size_t        pkgdepdb_pkg_filelist_get  (pkgdepdb_pkg*,
                                          const char**, size_t, size_t);
size_t        pkgdepdb_pkg_filelist_add  (pkgdepdb_pkg*, const char*);
size_t        pkgdepdb_pkg_filelist_del_s(pkgdepdb_pkg*, const char*);
size_t        pkgdepdb_pkg_filelist_del_i(pkgdepdb_pkg*, size_t);

size_t        pkgdepdb_pkg_info_count_keys  (pkgdepdb_pkg*);
size_t        pkgdepdb_pkg_info_get_keys    (pkgdepdb_pkg*, const char**, size_t, size_t);
size_t        pkgdepdb_pkg_info_count_values(pkgdepdb_pkg*, const char *key);
size_t        pkgdepdb_pkg_info_get_values  (pkgdepdb_pkg*, const char *key, const char**, size_t, size_t);
size_t        pkgdepdb_pkg_info_add  (pkgdepdb_pkg*, const char*, const char*);
size_t        pkgdepdb_pkg_info_del_s(pkgdepdb_pkg*, const char*, const char*);
size_t        pkgdepdb_pkg_info_del_i(pkgdepdb_pkg*, const char*, size_t);

size_t        pkgdepdb_pkg_elf_count(pkgdepdb_pkg*);
size_t        pkgdepdb_pkg_elf_get  (pkgdepdb_pkg*, pkgdepdb_elf*, size_t,
                                     size_t);
size_t        pkgdepdb_pkg_elf_add  (pkgdepdb_pkg*, pkgdepdb_elf);
size_t        pkgdepdb_pkg_elf_del_e(pkgdepdb_pkg*, pkgdepdb_elf);
size_t        pkgdepdb_pkg_elf_del_i(pkgdepdb_pkg*, size_t);

/* some exposed utility functions */
void          pkgdepdb_pkg_guess_version(pkgdepdb_pkg*, const char *filename);

pkgdepdb_bool pkgdepdb_pkg_conflict(pkgdepdb_pkg*, pkgdepdb_pkg*);
pkgdepdb_bool pkgdepdb_pkg_replaces(pkgdepdb_pkg*, pkgdepdb_pkg*);

/*********
 * pkgdepdb::Elf interface
 */

pkgdepdb_elf  pkgdepdb_elf_new  (void);
void          pkgdepdb_elf_unref(pkgdepdb_elf);
pkgdepdb_elf  pkgdepdb_elf_open (const char *file, int *err, pkgdepdb_cfg*);
pkgdepdb_elf  pkgdepdb_elf_read (const char *data, size_t size,
                                 const char *basename, const char *dirname,
                                 int *err, pkgdepdb_cfg*);

const char*   pkgdepdb_elf_dirname     (pkgdepdb_elf);
const char*   pkgdepdb_elf_basename    (pkgdepdb_elf);
void          pkgdepdb_elf_set_dirname (pkgdepdb_elf, const char*);
void          pkgdepdb_elf_set_basename(pkgdepdb_elf, const char*);

unsigned char pkgdepdb_elf_class(pkgdepdb_elf);
unsigned char pkgdepdb_elf_data (pkgdepdb_elf);
unsigned char pkgdepdb_elf_osabi(pkgdepdb_elf);
/* shouldn't really be set manually but what the heck... */
void          pkgdepdb_elf_set_class(pkgdepdb_elf, unsigned char);
void          pkgdepdb_elf_set_data (pkgdepdb_elf, unsigned char);
void          pkgdepdb_elf_set_osabi(pkgdepdb_elf, unsigned char);

/* convenience interface */
const char*   pkgdepdb_elf_class_string(pkgdepdb_elf);
const char*   pkgdepdb_elf_data_string (pkgdepdb_elf);
const char*   pkgdepdb_elf_osabi_string(pkgdepdb_elf);

const char*   pkgdepdb_elf_rpath      (pkgdepdb_elf);
const char*   pkgdepdb_elf_runpath    (pkgdepdb_elf);
const char*   pkgdepdb_elf_interpreter(pkgdepdb_elf);
/* also shouldn't be set manually */
void          pkgdepdb_elf_set_rpath      (pkgdepdb_elf, const char*);
void          pkgdepdb_elf_set_runpath    (pkgdepdb_elf, const char*);
void          pkgdepdb_elf_set_interpreter(pkgdepdb_elf, const char*);

size_t        pkgdepdb_elf_needed_count(pkgdepdb_elf);
size_t        pkgdepdb_elf_needed_get  (pkgdepdb_elf, const char**, size_t,
                                        size_t);

/* for completeness */
pkgdepdb_bool pkgdepdb_elf_needed_contains(pkgdepdb_elf, const char*);
void          pkgdepdb_elf_needed_add     (pkgdepdb_elf, const char*);
size_t        pkgdepdb_elf_needed_del_s   (pkgdepdb_elf, const char*);
void          pkgdepdb_elf_needed_del_i   (pkgdepdb_elf, size_t);

/* OSABI/class/data compatibility check */
int           pkgdepdb_elf_can_use(pkgdepdb_elf, pkgdepdb_elf obj, int strict);

#ifdef __cplusplus
} /* "C" */
#endif

#endif
