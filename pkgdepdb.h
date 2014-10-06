#ifndef PKGDEPDB_EXT_H__
#define PKGDEPDB_EXT_H__

#ifdef __cplusplus
extern "C" {
#endif

/* ELF objects are reference counted pointers */
typedef struct pkgdepdb_elf_*   pkgdepdb_elf;
typedef struct pkgdepdb_pkg_    pkgdepdb_pkg;
typedef struct pkgdepdb_db_     pkgdepdb_db;
typedef struct pkgdepdb_config_ pkgdepdb_config;

/*********
 * pkgdepdb::Config interface
 */
pkgdepdb_config* pkgdepdb_config_new   (void);
void             pkgdepdb_config_delete(pkgdepdb_config*);

int              pkgdepdb_config_load(pkgdepdb_config*, const char *filepath);
int              pkgdepdb_config_load_default(pkgdepdb_config*);

const char*      pkgdepdb_config_database     (pkgdepdb_config*);
void             pkgdepdb_config_set_database (pkgdepdb_config*, const char*);

unsigned int     pkgdepdb_config_verbosity    (pkgdepdb_config*);
void             pkgdepdb_config_set_verbosity(pkgdepdb_config*, unsigned int);

/* some boolean properties */
int              pkgdepdb_config_quiet                 (pkgdepdb_config*);
void             pkgdepdb_config_set_quiet             (pkgdepdb_config*, int);
int              pkgdepdb_config_package_depends       (pkgdepdb_config*);
void             pkgdepdb_config_set_package_depends   (pkgdepdb_config*, int);
int              pkgdepdb_config_package_file_lists    (pkgdepdb_config*);
void             pkgdepdb_config_set_package_file_lists(pkgdepdb_config*, int);

/* threading */
unsigned int     pkgdepdb_config_max_jobs    (pkgdepdb_config*);
void             pkgdepdb_config_set_max_jobs(pkgdepdb_config*, unsigned int);

/* output */
enum {
  PKGDEPDB_CONFIG_LOG_LEVEL_DEBUG,
  PKGDEPDB_CONFIG_LOG_LEVEL_MESSAGE,
  PKGDEPDB_CONFIG_LOG_LEVEL_PRINT,
  PKGDEPDB_CONFIG_LOG_LEVEL_WARN,
  PKGDEPDB_CONFIG_LOG_LEVEL_ERROR,
};
int    *pkgdepdb_config_log_level    (pkgdepdb_config*);
void   *pkgdepdb_config_set_log_level(pkgdepdb_config*, int);

/* json - this part of the interface should not usually be required... */
#define PKGDEPDB_JSONBITS_QUERY (1<<0)
#define PKGDEPDB_JSONBITS_DB    (1<<1)
int    *pkgdepdb_config_json    (pkgdepdb_config*);
void   *pkgdepdb_config_set_json(pkgdepdb_config*, int);

/*********
 * pkgdepdb::DB interface
 */

pkgdepdb_db *pkgdepdb_db_new   (pkgdepdb_config*);
void         pkgdepdb_db_delete(pkgdepdb_db*);
int          pkgdepdb_db_read  (pkgdepdb_db*, const char *filename);
int          pkgdepdb_db_store (pkgdepdb_db*, const char *filename);

unsigned int  pkgdepdb_db_loaded_version(pkgdepdb_db*);

unsigned int  pkgdepdb_db_strict_linking(pkgdepdb_db*);

const char*   pkgdepdb_db_name(pkgdepdb_db*);

size_t pkgdepdb_db_library_path_count(pkgdepdb_db*);
size_t pkgdepdb_db_library_path_get  (pkgdepdb_db*, const char**, size_t);
int    pkgdepdb_db_library_path_add  (pkgdepdb_db*, const char*);
int    pkgdepdb_db_library_path_del_s(pkgdepdb_db*, const char*);
int    pkgdepdb_db_library_path_del_i(pkgdepdb_db*, size_t);

size_t pkgdepdb_db_package_count(pkgdepdb_db*);
size_t pkgdepdb_db_object_count (pkgdepdb_db*);

size_t pkgdepdb_db_ignored_files_count(pkgdepdb_db*);
size_t pkgdepdb_db_ignored_files_get  (pkgdepdb_db*, const char**, size_t);
int    pkgdepdb_db_ignored_files_add  (pkgdepdb_db*, const char*);
int    pkgdepdb_db_ignored_files_del_s(pkgdepdb_db*, const char*);
int    pkgdepdb_db_ignored_files_del_i(pkgdepdb_db*, size_t);

size_t pkgdepdb_db_base_packages_count(pkgdepdb_db*);
size_t pkgdepdb_db_base_packages_get  (pkgdepdb_db*, const char**, size_t);
size_t pkgdepdb_db_base_packages_add  (pkgdepdb_db*, const char*);
int    pkgdepdb_db_base_packages_del_s(pkgdepdb_db*, const char*);
int    pkgdepdb_db_base_packages_del_i(pkgdepdb_db*, size_t);

size_t pkgdepdb_db_assume_found_count(pkgdepdb_db*);
size_t pkgdepdb_db_assume_found_get  (pkgdepdb_db*, const char**, size_t);
int    pkgdepdb_db_assume_found_add  (pkgdepdb_db*, const char*);
int    pkgdepdb_db_assume_found_del_s(pkgdepdb_db*, const char*);
int    pkgdepdb_db_assume_found_del_i(pkgdepdb_db*, size_t);

int    pkgdepdb_db_delete_package_s(pkgdepdb_db*, const char*);

void   pkgdepdb_db_relink_all     (pkgdepdb_db*);
void   pkgdepdb_db_fix_paths      (pkgdepdb_db*);
int    pkgdepdb_db_wipe_packages  (pkgdepdb_db*);
int    pkgdepdb_db_wipe_file_lists(pkgdepdb_db*);

/*********
 * pkgdepdb::Package interface
 */

pkgdepdb_pkg* pkgdepdb_pkg_new   (void);
void          pkgdepdb_pkg_delete(pkgdepdb_pkg*);

const char*   pkgdepdb_pkg_name   (pkgdepdb_pkg*);
const char*   pkgdepdb_pkg_version(pkgdepdb_pkg*);
void          pkgdepdb_pkg_set_name   (pkgdepdb_pkg*, const char*);
void          pkgdepdb_pkg_set_version(pkgdepdb_pkg*, const char*);

enum {
  PKGDEPDB_PKG_DEPENDS,
  PKGDEPDB_PKG_OPTDEPENDS,
  PKGDEPDB_PKG_MAKEDEPENDS,
  PKGDEPDB_PKG_PROVIDES,
  PKGDEPDB_PKG_CONFLICTS,
  PKGDEPDB_PKG_REPLACES,
};
size_t        pkgdepdb_pkg_dep_count   (pkgdepdb_pkg*, int what);
size_t        pkgdepdb_pkg_dep_get     (pkgdepdb_pkg*, int what, const char**,
                                        size_t, size_t);
void          pkgdepdb_pkg_dep_add     (pkgdepdb_pkg*, int what, const char*);
void          pkgdepdb_pkg_dep_del_name(pkgdepdb_pkg*, int what, const char*);
void          pkgdepdb_pkg_dep_del_full(pkgdepdb_pkg*, int what,
                                        const char*, const char*);
void          pkgdepdb_pkg_dep_del_i   (pkgdepdb_pkg*, int what, size_t);

size_t        pkgdepdb_pkg_groups_count(pkgdepdb_pkg*);
size_t        pkgdepdb_pkg_groups_get  (pkgdepdb_pkg*, const char**, size_t);
void          pkgdepdb_pkg_groups_add  (pkgdepdb_pkg*, const char*);
void          pkgdepdb_pkg_groups_del_s(pkgdepdb_pkg*, const char*);
void          pkgdepdb_pkg_groups_del_i(pkgdepdb_pkg*, size_t);

size_t        pkgdepdb_pkg_filelist_count(pkgdepdb_pkg*);
size_t        pkgdepdb_pkg_filelist_get  (pkgdepdb_pkg*,
                                          const char**, size_t, size_t);
void          pkgdepdb_pkg_filelist_add  (pkgdepdb_pkg*, const char*);
void          pkgdepdb_pkg_filelist_del_s(pkgdepdb_pkg*, const char*);
void          pkgdepdb_pkg_filelist_del_i(pkgdepdb_pkg*, size_t);


/*********
 * pkgdepdb::Elf interface
 */

pkgdepdb_elf  pkgdepdb_elf_new  (void);
void          pkgdepdb_elf_unref(pkgdepdb_elf);
pkgdepdb_elf  pkgdepdb_elf_open (const char *file, int *err, pkgdepdb_config*);
pkgdepdb_elf  pkgdepdb_elf_read (const char *data, size_t size,
                                 const char *basename, const char *dirname,
                                 int *err, pkgdepdb_config*);

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
size_t        pkgdepdb_elf_needed_get  (pkgdepdb_elf, const char**, size_t);

/* for completeness */
int           pkgdepdb_elf_needed_contains(pkgdepdb_elf, const char*);
void          pkgdepdb_elf_needed_add     (pkgdepdb_elf, const char*);
int           pkgdepdb_elf_needed_del_s   (pkgdepdb_elf, const char*);
void          pkgdepdb_elf_needed_del_i   (pkgdepdb_elf, size_t);

/* OSABI/class/data compatibility check */
int           pkgdepdb_elf_can_use(pkgdepdb_elf, pkgdepdb_elf obj, int strict);

#ifdef __cplusplus
} /* "C" */
#endif

#endif
