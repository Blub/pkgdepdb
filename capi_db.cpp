#include "main.h"
#include "elf.h"
#include "package.h"
#include "db.h"

#include "pkgdepdb.h"

using namespace pkgdepdb;

pkgdepdb_db *pkgdepdb_db_new(pkgdepdb_config *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return reinterpret_cast<pkgdepdb_db*>(new DB(*cfg));
}

void pkgdepdb_db_delete(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  delete db;
}

int pkgdepdb_db_read(pkgdepdb_db *db_, const char *filename) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->Read(filename);
}

int pkgdepdb_db_store(pkgdepdb_db *db_, const char *filename) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->Store(filename);
}

unsigned int pkgdepdb_db_loaded_version(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->loaded_version_;
}

unsigned int pkgdepdb_db_strict_linking(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->strict_linking_;
}

const char* pkgdepdb_db_name(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->name_.c_str();
}

size_t pkgdepdb_db_library_path_count(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->library_path_.size();
}

template<class STRINGLIST>
static size_t pkgdepdb_db_string_list(const DB& db, const char **o, size_t n,
                                      STRINGLIST DB::*member)
{
  size_t got = 0;
  for (const auto& i : db.*member) {
    if (!n--)
      return got;
    o[got++] = i.c_str();
  }
  return got;
}

size_t pkgdepdb_db_library_path_get(pkgdepdb_db *db_, const char** o, size_t n)
{
  auto db = reinterpret_cast<DB*>(db_);
  return pkgdepdb_db_string_list(*db, o, n, &DB::library_path_);
}

int pkgdepdb_db_library_path_add(pkgdepdb_db *db_, const char *path) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->LD_Append(path);
}

int pkgdepdb_db_library_path_del_s(pkgdepdb_db *db_, const char *path) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->LD_Delete(path);
}

int pkgdepdb_db_library_path_del_i(pkgdepdb_db *db_, size_t index) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->LD_Delete(index);
}

size_t pkgdepdb_db_package_count(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->packages_.size();
}

int pkgdepdb_db_delete_package_s(pkgdepdb_db *db_, const char *name) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->DeletePackage(name);
}

size_t pkgdepdb_db_object_count(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->objects_.size();
}

size_t pkgdepdb_db_ignored_files_count(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->ignore_file_rules_.size();
}

size_t pkgdepdb_db_ignored_files_get(pkgdepdb_db *d_, const char **o, size_t n)
{
  auto db = reinterpret_cast<DB*>(d_);
  return pkgdepdb_db_string_list(*db, o, n, &DB::ignore_file_rules_);
}

int pkgdepdb_db_ignored_files_add(pkgdepdb_db *db_, const char *file) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->IgnoreFile_Add(file);
}

int pkgdepdb_db_ignored_files_del_s(pkgdepdb_db *db_, const char *file) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->IgnoreFile_Delete(file);
}

int pkgdepdb_db_ignored_files_del_i(pkgdepdb_db *db_, size_t index) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->IgnoreFile_Delete(index);
}

size_t pkgdepdb_db_base_packages_count(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->base_packages_.size();
}

size_t pkgdepdb_db_base_packages_get(pkgdepdb_db *d_, const char **o, size_t n)
{
  auto db = reinterpret_cast<DB*>(d_);
  return pkgdepdb_db_string_list(*db, o, n, &DB::base_packages_);
}

size_t pkgdepdb_db_base_packages_add(pkgdepdb_db *db_, const char *name) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->BasePackages_Add(name);
}

int pkgdepdb_db_base_packages_del_s(pkgdepdb_db *db_, const char *name) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->BasePackages_Delete(name);
}

int pkgdepdb_db_base_packages_del_i(pkgdepdb_db *db_, size_t index) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->BasePackages_Delete(index);
}

size_t pkgdepdb_db_assume_found_count(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->assume_found_rules_.size();
}

size_t pkgdepdb_db_assume_found_get(pkgdepdb_db *db_, const char **o, size_t n)
{
  auto db = reinterpret_cast<DB*>(db_);
  return pkgdepdb_db_string_list(*db, o, n, &DB::assume_found_rules_);
}

int pkgdepdb_db_assume_found_add(pkgdepdb_db *db_, const char *lib) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->AssumeFound_Add(lib);
}

int pkgdepdb_db_assume_found_del_s(pkgdepdb_db *db_, const char *lib) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->AssumeFound_Delete(lib);
}

int pkgdepdb_db_assume_found_del_i(pkgdepdb_db *db_, size_t index) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->AssumeFound_Delete(index);
}

void pkgdepdb_db_relink_all(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->RelinkAll();
}

void pkgdepdb_db_fix_paths(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->FixPaths();
}

int pkgdepdb_db_wipe_packages(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->WipePackages();
}

int pkgdepdb_db_wipe_file_lists(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->WipeFilelists();
}
