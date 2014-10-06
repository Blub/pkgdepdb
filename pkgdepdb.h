#ifndef PKGDEPDB_EXT_H__
#define PKGDEPDB_EXT_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pkgdepdb_db_     pkgdepdb_db;
typedef struct pkgdepdb_config_ pkgdepdb_config;

/*********
 * pkgdepdb::Config interface
 */
pkgdepdb_config *pkgdepdb_config_new();
void             pkgdepdb_config_delete(pkgdepdb_config*);
int              pkgdepdb_config_load(pkgdepdb_config*, const char *filepath);
int              pkgdepdb_config_load_default(pkgdepdb_config*);
void             pkgdepdb_config_log(pkgdepdb_config*, int level,
                                     const char *format, ...);

const char      *pkgdepdb_config_database     (pkgdepdb_config*);
void             pkgdepdb_config_set_database (pkgdepdb_config*, const char*);

unsigned int    *pkgdepdb_config_verbosity    (pkgdepdb_config*);
void            *pkgdepdb_config_set_verbosity(pkgdepdb_config*, unsigned int);

/* some boolean properties */
int             *pkgdepdb_config_quiet                 (pkgdepdb_config*);
void            *pkgdepdb_config_set_quiet             (pkgdepdb_config*, int);
int             *pkgdepdb_config_package_depends       (pkgdepdb_config*);
void            *pkgdepdb_config_set_package_depends   (pkgdepdb_config*, int);
int             *pkgdepdb_config_package_file_lists    (pkgdepdb_config*);
void            *pkgdepdb_config_set_package_file_lists(pkgdepdb_config*, int);

/* threading */
unsigned int    *pkgdepdb_config_max_jobs    (pkgdepdb_config*);
void            *pkgdepdb_config_set_max_jobs(pkgdepdb_config*, unsigned int);

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

pkgdepdb_db *pkgdepdb_db_new   ();
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

void   pkgdepdb_db_delete_package_s(pkgdepdb_db*, const char*);

void   pkgdepdb_db_relink_all     (pkgdepdb_db*);
void   pkgdepdb_db_fix_paths      (pkgdepdb_db*);
void   pkgdepdb_db_wipe_packages  (pkgdepdb_db*);
void   pkgdepdb_db_wipe_file_lists(pkgdepdb_db*);

#ifdef __cplusplus
} /* "C" */
#endif

#endif
