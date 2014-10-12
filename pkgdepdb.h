#ifndef PKGDEPDB_EXT_H__
#define PKGDEPDB_EXT_H__

#ifdef __cplusplus
extern "C" {
#endif

/*********
 * common types
 */

typedef int pkgdepdb_bool;

/**
 * ELF objects are reference counted, so they have to be un-referenced after
 * use.
 */
typedef struct pkgdepdb_elf_* pkgdepdb_elf;

/**
 * Packages are owned. When they're inside a database, you must not destroy
 * the package. When taking it out of a database, it's your responsibility to
 * destroy it.
 * Installing the same object to multiple databases is not supported.
 */
typedef struct pkgdepdb_pkg_  pkgdepdb_pkg;

/**
 * Database. Packages and object lists are proxy objects, they cannot be
 * modified. Instead packages are installed or uninstalled, and objects are
 * part of packages.
 */
typedef struct pkgdepdb_db_   pkgdepdb_db;

/**
 * Configuration interface, used to control verbosity and threading behavior
 * of some functionality.
 */
typedef struct pkgdepdb_cfg_  pkgdepdb_cfg;

/*********
 * Main interface.
 * Calling init several times is supported but will clear out any previously
 * generated error messages.
 * Finalize should be called once at the end only.
 */

void          pkgdepdb_init(void);
const char*   pkgdepdb_error(void);
void          pkgdepdb_set_error(const char*, ...);
void          pkgdepdb_clear_error(void);
void          pkgdepdb_finalize(void);

/*********
 * pkgdepdb::Config interface
 */
/** Create a new configuration instance. */
pkgdepdb_cfg*    pkgdepdb_cfg_new   (void);

/** Delete a configuration instance.
 * \param s configuration instance to delete.
 */
void             pkgdepdb_cfg_delete(pkgdepdb_cfg *s);

/** Load a configuration file into the current instance.
 * \param s configuration instance.
 * \param file path to a configuration file to parse.
 * \returns true on success.
 */
pkgdepdb_bool    pkgdepdb_cfg_load        (pkgdepdb_cfg *s, const char *file);

/** Load the default configuration file from standard configuration paths.
 * \param s configuration instance.
 * \returns true on success.
 */
pkgdepdb_bool    pkgdepdb_cfg_load_default(pkgdepdb_cfg *s);

/** Parse a configuration text string.
 * \param s configuration instance.
 * \param name the name to use in error messages.
 * \param data the configuration text data.
 * \param length length of the data to parse.
 * \returns true on success.
 */
pkgdepdb_bool    pkgdepdb_cfg_read        (pkgdepdb_cfg *s, const char *name,
                                           const char *data, size_t length);

/** The configuration's database string. Used by tools to load a default db. */
const char*      pkgdepdb_cfg_database     (pkgdepdb_cfg*);
/** Set the configuration's database string. */
void             pkgdepdb_cfg_set_database (pkgdepdb_cfg*, const char*);

/** Get the verbosity. */
unsigned int     pkgdepdb_cfg_verbosity    (pkgdepdb_cfg*);
/** Set the verbosity. */
void             pkgdepdb_cfg_set_verbosity(pkgdepdb_cfg*, unsigned int);

/** Get the 'quiet' setting. */
pkgdepdb_bool    pkgdepdb_cfg_quiet                 (pkgdepdb_cfg*);
/** Set the 'quiet' setting. */
void             pkgdepdb_cfg_set_quiet             (pkgdepdb_cfg*,
                                                     pkgdepdb_bool);
/**
 * The package_depends setting controls whether dependency information is
 * supposed to be stored in the database.
 */
pkgdepdb_bool    pkgdepdb_cfg_package_depends       (pkgdepdb_cfg*);
/** Change whether dependency information will be stored in the database.
 * \sa pkgdepdb_cfg_package_depends()
 */
void             pkgdepdb_cfg_set_package_depends   (pkgdepdb_cfg*,
                                                     pkgdepdb_bool);
/** Check whether a pacakge's list of files will be stored. */
pkgdepdb_bool    pkgdepdb_cfg_package_file_lists    (pkgdepdb_cfg*);
/** Controls whether a package's list of files should be stored. */
void             pkgdepdb_cfg_set_package_file_lists(pkgdepdb_cfg*,
                                                     pkgdepdb_bool);
/** Check whether unrecognized PKGINFO strings are stored in the info array. */
pkgdepdb_bool    pkgdepdb_cfg_package_info          (pkgdepdb_cfg*);
/** Whether unrecognized PKGINFO strings are stored in the info array. */
void             pkgdepdb_cfg_set_package_info      (pkgdepdb_cfg*,
                                                     pkgdepdb_bool);

/** Check the amount of maximum threads allowed for threaded operations. */
unsigned int     pkgdepdb_cfg_max_jobs    (pkgdepdb_cfg*);
/** Set the amount of maximum threads allowed for threaded operations. */
void             pkgdepdb_cfg_set_max_jobs(pkgdepdb_cfg*, unsigned int);

/**
 * The log level controls which types of messages to print to the terminal.
 */
enum PKGDEPDB_CFG_LOG_LEVEL {
  PKGDEPDB_CFG_LOG_LEVEL_DEBUG,   /**< pring all messages, even debug info */
  PKGDEPDB_CFG_LOG_LEVEL_MESSAGE, /**< print all messages but debug info */
  PKGDEPDB_CFG_LOG_LEVEL_PRINT,   /**< messages warnings and errors */
  PKGDEPDB_CFG_LOG_LEVEL_WARN,    /**< print warnings */
  PKGDEPDB_CFG_LOG_LEVEL_ERROR,   /**< pring only errors to the terminal */
};
/** check the current log level. \sa PKGDEPDB_CFG_LOG_LEVEL */
unsigned int     pkgdepdb_cfg_log_level    (pkgdepdb_cfg*);
/** set the current log level. \sa PKGDEPDB_CFG_LOG_LEVEL  */
void             pkgdepdb_cfg_set_log_level(pkgdepdb_cfg*, unsigned int);

/* json - this part of the interface should not usually be required... */
/** used only by the commandline tool */
#define PKGDEPDB_JSONBITS_QUERY (1<<0)
/** used only by the commandline tool */
#define PKGDEPDB_JSONBITS_DB    (1<<1)
/** used only by the commandline tool */
unsigned int     pkgdepdb_cfg_json    (pkgdepdb_cfg*);
/** used only by the commandline tool */
void             pkgdepdb_cfg_set_json(pkgdepdb_cfg*, unsigned int);

/*********
 * pkgdepdb::DB interface
 */

/**
 * Create a fresh empty database instance and link it to the provided
 * configuration. The configuration instance must remain valid as long as the
 * database uses it, it can be modified between operations with the changing
 * taking effect on any future operations performed ont he database instance.
 * \param cfg configuration instance, must stay valid until the db is deleted.
 * \returns a new Database instance.
 */
pkgdepdb_db  *pkgdepdb_db_new   (pkgdepdb_cfg *cfg);
/** Delete a database instance. */
void          pkgdepdb_db_delete(pkgdepdb_db*);
/** Read a database from disk.
 * \param db the database instance.
 * \param filename path to the database file to read.
 * \returns true on success.
 */
pkgdepdb_bool pkgdepdb_db_read  (pkgdepdb_db *db, const char *filename);
/** Store the database to disk.
 * \param db the database instance.
 * \param filename path to write the database to.
 * \returns true on success.
 */
pkgdepdb_bool pkgdepdb_db_store (pkgdepdb_db *db, const char *filename);

/** After a database was loaded from disk, this reflects the data-format
 * version number.
 */
unsigned int  pkgdepdb_db_loaded_version(pkgdepdb_db*);

/** Return whether database queries should assume strict link mode.
 * \sa pkgdepdb_set_strict_linking().
 */
pkgdepdb_bool pkgdepdb_db_strict_linking(pkgdepdb_db*);
/** Set strict-linking mode.
 * In strict mode, a 'NONE' OSABI value is treated like its own ABI.
 * If strict mode is disabled, ELF files of any ABI can link against a
 * 'NONE' abi.
 */
void          pkgdepdb_db_set_strict_linking(pkgdepdb_db*, pkgdepdb_bool);

/** Query the database name. \sa pkgdepdb_db_set_name(). */
const char*   pkgdepdb_db_name(pkgdepdb_db*);
/** Set the database name. The name has absolutely no effect on any operation
 * and exists only for convenience.
 */
void          pkgdepdb_db_set_name(pkgdepdb_db*, const char*);

/** Return the number of configured library path entries. */
size_t        pkgdepdb_db_library_path_count   (pkgdepdb_db*);
/** Retrieve library path entries.
 * The retrieved character pointers must NOT be freed, they will be valid only
 * until any write operation is performed on the database.
 * \param db the database instance.
 * \param out A list of at least 'count' pointers to C strings which will be
 *            filled with pointers valid until the next write operation on the
 *            database occurs, they must NOT be freed manually.
 * \param offset How many entries to skip ahead. The first string ending up in
 *               the output array will be entry number 'offset'. Specifying an
 *               out of bounds number will result in zero entries returned.
 * \param count Number of entries to retrieve.
 * \returns the number of entries successfully stored in the output array.
 */
size_t        pkgdepdb_db_library_path_get     (pkgdepdb_db *db,
                                                const char **out,
                                                size_t offset, size_t count);
/** Check whether the database's library path contains a certain entry. */
pkgdepdb_bool pkgdepdb_db_library_path_contains(pkgdepdb_db*, const char*);
/** Add an entry to the database's library path array. */
pkgdepdb_bool pkgdepdb_db_library_path_add     (pkgdepdb_db*, const char*);
/** Delete the first library path exactly matching the provided string. */
pkgdepdb_bool pkgdepdb_db_library_path_del_s   (pkgdepdb_db*, const char*);
/** Delete a library path entry by index.
 * \returns true on success, false if the index was out of bounds.
 */
pkgdepdb_bool pkgdepdb_db_library_path_del_i   (pkgdepdb_db*, size_t);
/** Delete a range of library path entries. Out of bounds ranges are handled
 * correctly, the return value will reflect the actual number of entries
 * deleted.
 * \param db the database instance.
 * \param first the index of the first entry to remove.
 * \param count the number of entries to remove.
 * \returns number of entries removed.
 */
size_t        pkgdepdb_db_library_path_del_r   (pkgdepdb_db *db,
                                                size_t first, size_t, count);
/** Change a library path entry by index.
 * \returns true on success, false if the index was out of bounds.
 */
pkgdepdb_bool pkgdepdb_db_library_path_set_i   (pkgdepdb_db*, size_t,
                                                const char*);

/* the package delete functions "uninstall" the package from the db */
/** Retrieve the number of packages installed in the database. */
size_t        pkgdepdb_db_package_count   (pkgdepdb_db*);
/** Install a package into the database.
 * \returns true on success.
 */
pkgdepdb_bool pkgdepdb_db_package_install (pkgdepdb_db*, pkgdepdb_pkg*);
/** Find an installed package by name. */
pkgdepdb_pkg* pkgdepdb_db_package_find    (pkgdepdb_db*, const char*);
/** Retrieve a range of installed packages.
 * \param db the database instance.
 * \param out the output array that will be filled.
 * \param index the start index to copy from.
 * \param count the number of packages to retrieve.
 * \returns the number of packages actually stored in the output array.
 */
size_t        pkgdepdb_db_package_get     (pkgdepdb_db *db, pkgdepdb_pkg **out,
                                           size_t index, size_t count);
/** Delete and destroy package from the database. This calls
 * pkgdepdb_pkg_delete on the deleted package. All references to the package
 * are invalidated. */
pkgdepdb_bool pkgdepdb_db_package_delete_p(pkgdepdb_db*, pkgdepdb_pkg*);
/** Delete and destroy an installed package by name. All remaining references
 * to the package are invalidated. */
pkgdepdb_bool pkgdepdb_db_package_delete_s(pkgdepdb_db*, const char*);
/** Delete and destroy an installed package by index. All remaining references
 * to the package are invalidated. */
pkgdepdb_bool pkgdepdb_db_package_delete_i(pkgdepdb_db*, size_t);
/** Uninstall a package from the database without destroying the package
 * structure. Only a version taking a package reference is provided because
 * you have to manually call pkgdepdb_pkg_delete() on it afterwards.
 */
pkgdepdb_bool pkgdepdb_db_package_remove(pkgdepdb_db*, pkgdepdb_pkg*);

/** Check whether an installed package has to be considered broken in the
 * database.
 * Currently this means that the package contains an ELF file which fails to
 * find at least one required library with respect to the database's and the
 * package's library path, as well as the file's rpath and runapth properties.
 */
pkgdepdb_bool pkgdepdb_db_package_is_broken(pkgdepdb_db*, pkgdepdb_pkg*);

/** Retrieve the number of ELF files contained within all the packages
 * installed into the database. These have no particular order. */
size_t pkgdepdb_db_object_count(pkgdepdb_db*);
/** Retrieve a range of ELF files from the database. No assumptions about their
 * order can be made. Any write access to the database may cause the order
 * to change.
 * \param db the database instance.
 * \param out pointer to an array of ELF references, valid existing reference
 *            objects will be correctly replaced. Meaning the list may contain
 *            intermixed valid references and NULL pointers, and no more values
 *            than specified by the return value will be touched.
 * \param index the index of the first element to retrieve.
 * \param count the number of elements that fit into the output array.
 * \returns the number of elements retrieved. You must manually calls
 *          pkgdepdb_elf_unref() on the retrieved elements.
 */
size_t pkgdepdb_db_object_get(pkgdepdb_db *db, pkgdepdb_elf *out,
                              size_t index, size_t count);

/** Check whether an ELF file which is part of a package inside the specified
 * database has to be considered broken with respect to the database's
 * and package's library path, as well as the file's rpath and runpath
 * settings.
 */
pkgdepdb_bool pkgdepdb_db_object_is_broken(pkgdepdb_db*, pkgdepdb_elf);

/** Retrieve the number of ignore-file rules in the database. */
size_t        pkgdepdb_db_ignored_files_count   (pkgdepdb_db*);
/** Retrieve ignore-file rules. The parameters work exactly like the ones of
 * pkgdepdb_db_library_path_get(). */
size_t        pkgdepdb_db_ignored_files_get     (pkgdepdb_db*, const char**,
                                                 size_t, size_t);
/** Add a new ignore-file rule to the database. */
pkgdepdb_bool pkgdepdb_db_ignored_files_add     (pkgdepdb_db*, const char*);
/** Check whether an entry is listed in the db's ignore file rules. */
pkgdepdb_bool pkgdepdb_db_ignored_files_contains(pkgdepdb_db*, const char*);
/** Delete the frist ignore-file entry matching the specified string. */
pkgdepdb_bool pkgdepdb_db_ignored_files_del_s   (pkgdepdb_db*, const char*);
/** Delete an ignore-file entry by index. */
pkgdepdb_bool pkgdepdb_db_ignored_files_del_i   (pkgdepdb_db*, size_t);
/** Delete a range of ignore-file entries.
 * \returns the number of entries actually deleted after bounds checking. */
size_t        pkgdepdb_db_ignored_files_del_r   (pkgdepdb_db*, size_t, size_t);

/** Access the base packages list: \sa pkgdepdb_db_library_path_count(). */
size_t        pkgdepdb_db_base_packages_count   (pkgdepdb_db*);
/** Access the base packages list: \sa pkgdepdb_db_library_path_get(). */
size_t        pkgdepdb_db_base_packages_get     (pkgdepdb_db*, const char**,
                                                 size_t, size_t);
/** Access the base packages list: \sa pkgdepdb_db_library_path_contains(). */
pkgdepdb_bool pkgdepdb_db_base_packages_contains(pkgdepdb_db*, const char*);
/** Access the base packages list: \sa pkgdepdb_db_library_path_add(). */
size_t        pkgdepdb_db_base_packages_add     (pkgdepdb_db*, const char*);
/** Access the base packages list: \sa pkgdepdb_db_library_path_del_s(). */
pkgdepdb_bool pkgdepdb_db_base_packages_del_s   (pkgdepdb_db*, const char*);
/** Access the base packages list: \sa pkgdepdb_db_library_path_del_i(). */
pkgdepdb_bool pkgdepdb_db_base_packages_del_i   (pkgdepdb_db*, size_t);
/** Access the base packages list: \sa pkgdepdb_db_library_path_del_r(). */
size_t        pkgdepdb_db_base_packages_del_r   (pkgdepdb_db*, size_t, size_t);

/** List of libraries assumed to exist: \sa pkgdepdb_db_library_path_count().*/
size_t        pkgdepdb_db_assume_found_count   (pkgdepdb_db*);
/** List of libraries assumed to exist: \sa pkgdepdb_db_library_path_get().*/
size_t        pkgdepdb_db_assume_found_get     (pkgdepdb_db*, const char**,
                                                 size_t, size_t);
/** List of libraries assumed to exist: \sa pkgdepdb_db_library_path_add().*/
pkgdepdb_bool pkgdepdb_db_assume_found_add     (pkgdepdb_db*, const char*);
/** List of libraries assumed to exist:
 * \sa pkgdepdb_db_library_path_contains().*/
pkgdepdb_bool pkgdepdb_db_assume_found_contains(pkgdepdb_db*, const char*);
/** List of libraries assumed to exist: \sa pkgdepdb_db_library_path_del_s().*/
pkgdepdb_bool pkgdepdb_db_assume_found_del_s   (pkgdepdb_db*, const char*);
/** List of libraries assumed to exist: \sa pkgdepdb_db_library_path_del_i().*/
pkgdepdb_bool pkgdepdb_db_assume_found_del_i   (pkgdepdb_db*, size_t);
/** List of libraries assumed to exist: \sa pkgdepdb_db_library_path_del_r().*/
size_t        pkgdepdb_db_assume_found_del_r   (pkgdepdb_db*, size_t, size_t);

/**
 * Relink all objects contained in the database. Do this whenever you change
 * database rules such as the global library path.
 * This operation can use threading if the configuration enables it.
 */
void          pkgdepdb_db_relink_all    (pkgdepdb_db*);
void          pkgdepdb_db_fix_paths     (pkgdepdb_db*);
/** Convenience function to delete all packages from the database while keeping
 * all the rules. */
pkgdepdb_bool pkgdepdb_db_wipe_packages (pkgdepdb_db*);
/** Convenience function to delete the file lists of all packages in the db. */
pkgdepdb_bool pkgdepdb_db_wipe_filelists(pkgdepdb_db*);

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
  PKGDEPDB_PKG_CHECKDEPENDS,
  PKGDEPDB_PKG_PROVIDES,
  PKGDEPDB_PKG_CONFLICTS,
  PKGDEPDB_PKG_REPLACES,
};
size_t        pkgdepdb_pkg_dep_count   (pkgdepdb_pkg*, unsigned int what);
size_t        pkgdepdb_pkg_dep_get     (pkgdepdb_pkg*, unsigned int what,
                                        const char**, const char**, size_t,
                                        size_t);
pkgdepdb_bool pkgdepdb_pkg_dep_contains(pkgdepdb_pkg*, unsigned int what,
                                        const char*);
size_t        pkgdepdb_pkg_dep_add     (pkgdepdb_pkg*, unsigned int what,
                                        const char*, const char*);
size_t        pkgdepdb_pkg_dep_del_name(pkgdepdb_pkg*, unsigned int what,
                                        const char*);
size_t        pkgdepdb_pkg_dep_del_full(pkgdepdb_pkg*, unsigned int what,
                                        const char*, const char*);
size_t        pkgdepdb_pkg_dep_del_i   (pkgdepdb_pkg*, unsigned int what,
                                        size_t);
size_t        pkgdepdb_pkg_dep_del_r   (pkgdepdb_pkg*, unsigned int what,
                                        size_t, size_t);

size_t        pkgdepdb_pkg_groups_count   (pkgdepdb_pkg*);
size_t        pkgdepdb_pkg_groups_get     (pkgdepdb_pkg*, const char**, size_t,
                                           size_t);
pkgdepdb_bool pkgdepdb_pkg_groups_contains(pkgdepdb_pkg*, const char*);
size_t        pkgdepdb_pkg_groups_add     (pkgdepdb_pkg*, const char*);
size_t        pkgdepdb_pkg_groups_del_s   (pkgdepdb_pkg*, const char*);
size_t        pkgdepdb_pkg_groups_del_i   (pkgdepdb_pkg*, size_t);
size_t        pkgdepdb_pkg_groups_del_r   (pkgdepdb_pkg*, size_t, size_t);

size_t        pkgdepdb_pkg_filelist_count   (pkgdepdb_pkg*);
size_t        pkgdepdb_pkg_filelist_get     (pkgdepdb_pkg*,
                                             const char**, size_t, size_t);
pkgdepdb_bool pkgdepdb_pkg_filelist_contains(pkgdepdb_pkg*, const char*);
size_t        pkgdepdb_pkg_filelist_add     (pkgdepdb_pkg*, const char*);
size_t        pkgdepdb_pkg_filelist_del_s   (pkgdepdb_pkg*, const char*);
size_t        pkgdepdb_pkg_filelist_del_i   (pkgdepdb_pkg*, size_t);
size_t        pkgdepdb_pkg_filelist_del_r   (pkgdepdb_pkg*, size_t, size_t);
pkgdepdb_bool pkgdepdb_pkg_filelist_set_i   (pkgdepdb_pkg*, size_t,
                                             const char*);

size_t        pkgdepdb_pkg_info_count_keys  (pkgdepdb_pkg*);
size_t        pkgdepdb_pkg_info_get_keys    (pkgdepdb_pkg*, const char**,
                                             size_t, size_t);
pkgdepdb_bool pkgdepdb_pkg_info_contains_key(pkgdepdb_pkg*, const char*);
size_t        pkgdepdb_pkg_info_count_values(pkgdepdb_pkg*, const char *key);
size_t        pkgdepdb_pkg_info_get_values  (pkgdepdb_pkg*, const char *key,
                                             const char**, size_t, size_t);
size_t        pkgdepdb_pkg_info_add  (pkgdepdb_pkg*, const char*, const char*);
size_t        pkgdepdb_pkg_info_del_s(pkgdepdb_pkg*, const char*, const char*);
size_t        pkgdepdb_pkg_info_del_i(pkgdepdb_pkg*, const char*, size_t);
size_t        pkgdepdb_pkg_info_del_r(pkgdepdb_pkg*, const char*, size_t,
                                      size_t);
pkgdepdb_bool pkgdepdb_pkg_info_set_i(pkgdepdb_pkg*, const char*, size_t,
                                      const char*);

size_t        pkgdepdb_pkg_elf_count(pkgdepdb_pkg*);
size_t        pkgdepdb_pkg_elf_get  (pkgdepdb_pkg*, pkgdepdb_elf*, size_t,
                                     size_t);
size_t        pkgdepdb_pkg_elf_add  (pkgdepdb_pkg*, pkgdepdb_elf);
size_t        pkgdepdb_pkg_elf_del_e(pkgdepdb_pkg*, pkgdepdb_elf);
size_t        pkgdepdb_pkg_elf_del_i(pkgdepdb_pkg*, size_t);
size_t        pkgdepdb_pkg_elf_del_r(pkgdepdb_pkg*, size_t, size_t);
/* 0: Invalid index, 1: replaced, -1: deleted */
int           pkgdepdb_pkg_elf_set_i(pkgdepdb_pkg*, size_t, pkgdepdb_elf);

/* some exposed utility functions */
void          pkgdepdb_pkg_guess(pkgdepdb_pkg*, const char *filename);

pkgdepdb_bool pkgdepdb_pkg_conflict(pkgdepdb_pkg*, pkgdepdb_pkg*);
pkgdepdb_bool pkgdepdb_pkg_replaces(pkgdepdb_pkg*, pkgdepdb_pkg*);

/*********
 * pkgdepdb::Elf interface
 */

pkgdepdb_elf  pkgdepdb_elf_new  (void);
void          pkgdepdb_elf_unref(pkgdepdb_elf);
pkgdepdb_elf  pkgdepdb_elf_load (const char *file, int *err, pkgdepdb_cfg*);
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
pkgdepdb_bool pkgdepdb_elf_needed_contains(pkgdepdb_elf, const char*);
void          pkgdepdb_elf_needed_add     (pkgdepdb_elf, const char*);
size_t        pkgdepdb_elf_needed_del_s   (pkgdepdb_elf, const char*);
void          pkgdepdb_elf_needed_del_i   (pkgdepdb_elf, size_t);
void          pkgdepdb_elf_needed_del_r   (pkgdepdb_elf, size_t, size_t);

size_t        pkgdepdb_elf_missing_count   (pkgdepdb_elf);
size_t        pkgdepdb_elf_missing_get     (pkgdepdb_elf, const char**, size_t,
                                            size_t);
pkgdepdb_bool pkgdepdb_elf_missing_contains(pkgdepdb_elf, const char*);

size_t        pkgdepdb_elf_found_count   (pkgdepdb_elf);
size_t        pkgdepdb_elf_found_get     (pkgdepdb_elf, pkgdepdb_elf*, size_t,
                                          size_t);
pkgdepdb_elf  pkgdepdb_elf_found_find    (pkgdepdb_elf, const char*);

/* OSABI/class/data compatibility check */
int           pkgdepdb_elf_can_use(pkgdepdb_elf, pkgdepdb_elf obj, int strict);

#ifdef __cplusplus
} /* "C" */
#endif

#endif
