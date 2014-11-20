#include "main.h"
#include "elf.h"
#include "package.h"
#include "db.h"

#include "pkgdepdb.h"

#include "capi_algorithm.h"

using namespace pkgdepdb;

extern "C" {

pkgdepdb_db *pkgdepdb_db_new(pkgdepdb_cfg *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return reinterpret_cast<pkgdepdb_db*>(new DB(*cfg));
}

void pkgdepdb_db_delete(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  delete db;
}

pkgdepdb_bool pkgdepdb_db_load(pkgdepdb_db *db_, const char *filename) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->Load(filename);
}

pkgdepdb_bool pkgdepdb_db_store(pkgdepdb_db *db_, const char *filename) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->Store(filename);
}

unsigned int pkgdepdb_db_loaded_version(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->loaded_version_;
}

pkgdepdb_bool pkgdepdb_db_strict_linking(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->strict_linking_;
}

void pkgdepdb_db_set_strict_linking(pkgdepdb_db *db_, pkgdepdb_bool v) {
  auto db = reinterpret_cast<DB*>(db_);
  db->strict_linking_ = v;
}

const char* pkgdepdb_db_name(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->name_.c_str();
}
void pkgdepdb_db_set_name(pkgdepdb_db *db_, const char *v) {
  auto db = reinterpret_cast<DB*>(db_);
  db->name_ = v;
}

size_t pkgdepdb_db_library_path_count(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->library_path_.size();
}

size_t pkgdepdb_db_library_path_get(pkgdepdb_db *db_, const char **out,
                                    size_t off, size_t count)
{
  auto db = reinterpret_cast<DB*>(db_);
  return pkgdepdb_strlist_get(*db, &DB::library_path_, out, off, count);
}

pkgdepdb_bool pkgdepdb_db_library_path_add(pkgdepdb_db *db_, const char *path) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->LD_Append(path);
}

pkgdepdb_bool pkgdepdb_db_library_path_contains(pkgdepdb_db *db_,
                                                const char *path)
{
  auto db = reinterpret_cast<DB*>(db_);
  return pkgdepdb_strlist_contains(db->library_path_, path);
}

pkgdepdb_bool pkgdepdb_db_library_path_del_s(pkgdepdb_db *db_, const char *path) {
  auto db = reinterpret_cast<DB*>(db_);
  return pkgdepdb_strlist_del_s_one(db->library_path_, path) == 1;
}

pkgdepdb_bool pkgdepdb_db_library_path_del_i(pkgdepdb_db *db_, size_t index) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->LD_Delete(index);
}

size_t pkgdepdb_db_library_path_del_r(pkgdepdb_db *db_, size_t index,
                                      size_t count)
{
  auto db = reinterpret_cast<DB*>(db_);
  return pkgdepdb_strlist_del_r(db->library_path_, index, count);
}

pkgdepdb_bool pkgdepdb_db_library_path_set_i(pkgdepdb_db *db_, size_t index,
                                             const char *v)
{
  auto db = reinterpret_cast<DB*>(db_);
  return pkgdepdb_strlist_set_i(db->library_path_, index, v);
}

size_t pkgdepdb_db_package_count(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->packages_.size();
}

pkgdepdb_pkg* pkgdepdb_db_package_find(pkgdepdb_db *db_, const char *name) {
  auto db = reinterpret_cast<DB*>(db_);
  return reinterpret_cast<pkgdepdb_pkg*>(db->FindPkg(name));
}

size_t pkgdepdb_db_package_get(pkgdepdb_db *db_, pkgdepdb_pkg **out,
                               size_t off, size_t count)
{
  auto db = reinterpret_cast<DB*>(db_);
  auto len = db->packages_.size();
  if (off >= len)
    return 0;
  size_t got = 0;
  if (count > len - off)
    count = len - off;
  while (got < count)
    out[got++] = reinterpret_cast<pkgdepdb_pkg*>(db->packages_[off++]);
  return got;
}

pkgdepdb_bool pkgdepdb_db_package_install(pkgdepdb_db *db_, pkgdepdb_pkg *pkg_)
{
  auto db = reinterpret_cast<DB*>(db_);
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return db->InstallPackage(move(pkg)) ? 1 : 0;
}

pkgdepdb_bool pkgdepdb_db_package_delete_p(pkgdepdb_db *db_,
                                           pkgdepdb_pkg *pkg_)
{
  auto db = reinterpret_cast<DB*>(db_);
  auto pkg = reinterpret_cast<Package*>(pkg_);
  auto pkgiter = std::find(db->packages_.begin(), db->packages_.end(), pkg);
  return db->DeletePackage(pkgiter) ? 1 : 0;
}

pkgdepdb_bool pkgdepdb_db_package_remove(pkgdepdb_db *db_, pkgdepdb_pkg *pkg_)
{
  auto db = reinterpret_cast<DB*>(db_);
  auto pkg = reinterpret_cast<Package*>(pkg_);
  auto pkgiter = std::find(db->packages_.begin(), db->packages_.end(), pkg);
  return db->DeletePackage(pkgiter, false) ? 1 : 0;
}

pkgdepdb_bool pkgdepdb_db_package_delete_i(pkgdepdb_db *db_, size_t index) {
  auto db = reinterpret_cast<DB*>(db_);
  if (index >= db->packages_.size())
    return 0;
  return db->DeletePackage(db->packages_.begin() + index) ? 1 : 0;
}

pkgdepdb_bool pkgdepdb_db_package_delete_s(pkgdepdb_db *db_, const char *name)
{
  auto db = reinterpret_cast<DB*>(db_);
  return db->DeletePackage(name) ? 1 : 0;
}

pkgdepdb_bool pkgdepdb_db_package_is_broken(pkgdepdb_db *db_,
                                            pkgdepdb_pkg *pkg_)
{
  auto db = reinterpret_cast<DB*>(db_);
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return db->IsBroken(pkg) ? 1 : 0;
}

size_t pkgdepdb_db_object_count(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->objects_.size();
}

size_t pkgdepdb_db_object_get(pkgdepdb_db *db_, pkgdepdb_elf *out,
                              size_t off, size_t count)
{
  auto db = reinterpret_cast<DB*>(db_);
  if (off >= db->objects_.size())
    return 0;
  size_t got = 0;
  for (auto i = db->objects_.begin() + off; i != db->objects_.end(); ++i) {
    if (!count--)
      return got;
    rptr<Elf> **dest = reinterpret_cast<rptr<Elf>**>(&out[got++]);
    if (*dest)
      **dest = *i;
    else
      *dest = new rptr<Elf>(*i);
  }
  return got;
}

size_t pkgdepdb_db_ignored_files_count(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->ignore_file_rules_.size();
}

size_t pkgdepdb_db_ignored_files_get(pkgdepdb_db *d_, const char **out,
                                     size_t off, size_t count)
{
  auto db = reinterpret_cast<DB*>(d_);
  return pkgdepdb_strlist_get(*db, &DB::ignore_file_rules_, out, off, count);
}

pkgdepdb_bool pkgdepdb_db_ignored_files_add(pkgdepdb_db *db_, const char *file)
{
  auto db = reinterpret_cast<DB*>(db_);
  return db->IgnoreFile_Add(file);
}

pkgdepdb_bool pkgdepdb_db_ignored_files_contains(pkgdepdb_db *db_,
                                                 const char *file)
{
  auto db = reinterpret_cast<DB*>(db_);
  return pkgdepdb_strlist_contains(db->ignore_file_rules_, file);
}

pkgdepdb_bool pkgdepdb_db_ignored_files_del_s(pkgdepdb_db *db_,
                                              const char *file)
{
  auto db = reinterpret_cast<DB*>(db_);
  return db->IgnoreFile_Delete(file);
}

pkgdepdb_bool pkgdepdb_db_ignored_files_del_i(pkgdepdb_db *db_, size_t index) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->IgnoreFile_Delete(index);
}

size_t pkgdepdb_db_ignored_files_del_r(pkgdepdb_db *db_, size_t index,
                                       size_t count)
{
  auto db = reinterpret_cast<DB*>(db_);
  return pkgdepdb_strlist_del_r(db->ignore_file_rules_, index, count);
}

size_t pkgdepdb_db_base_packages_count(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->base_packages_.size();
}

size_t pkgdepdb_db_base_packages_get(pkgdepdb_db *d_, const char **out,
                                     size_t off, size_t count)
{
  auto db = reinterpret_cast<DB*>(d_);
  return pkgdepdb_strlist_get(*db, &DB::base_packages_, out, off, count);
}

size_t pkgdepdb_db_base_packages_add(pkgdepdb_db *db_, const char *name) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->BasePackages_Add(name);
}

pkgdepdb_bool pkgdepdb_db_base_packages_contains(pkgdepdb_db *db_,
                                                 const char *name)
{
  auto db = reinterpret_cast<DB*>(db_);
  return pkgdepdb_strlist_contains(db->base_packages_, name);
}

pkgdepdb_bool pkgdepdb_db_base_packages_del_s(pkgdepdb_db *db_,
                                              const char *name)
{
  auto db = reinterpret_cast<DB*>(db_);
  return db->BasePackages_Delete(name);
}

pkgdepdb_bool pkgdepdb_db_base_packages_del_i(pkgdepdb_db *db_, size_t index) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->BasePackages_Delete(index);
}

size_t pkgdepdb_db_base_packages_del_r(pkgdepdb_db *db_, size_t index,
                                       size_t count)
{
  auto db = reinterpret_cast<DB*>(db_);
  return pkgdepdb_strlist_del_r(db->base_packages_, index, count);
}

size_t pkgdepdb_db_assume_found_count(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->assume_found_rules_.size();
}

size_t pkgdepdb_db_assume_found_get(pkgdepdb_db *db_, const char **out,
                                     size_t off, size_t count)
{
  auto db = reinterpret_cast<DB*>(db_);
  return pkgdepdb_strlist_get(*db, &DB::assume_found_rules_, out, off, count);
}

pkgdepdb_bool pkgdepdb_db_assume_found_add(pkgdepdb_db *db_, const char *lib) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->AssumeFound_Add(lib);
}

pkgdepdb_bool pkgdepdb_db_assume_found_contains(pkgdepdb_db *db_,
                                                const char *lib)
{
  auto db = reinterpret_cast<DB*>(db_);
  return pkgdepdb_strlist_contains(db->assume_found_rules_, lib);
}

pkgdepdb_bool pkgdepdb_db_assume_found_del_s(pkgdepdb_db *db_, const char *lib)
{
  auto db = reinterpret_cast<DB*>(db_);
  return db->AssumeFound_Delete(lib);
}

pkgdepdb_bool pkgdepdb_db_assume_found_del_i(pkgdepdb_db *db_, size_t index) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->AssumeFound_Delete(index);
}

size_t pkgdepdb_db_assume_found_del_r(pkgdepdb_db *db_, size_t index,
                                      size_t count)
{
  auto db = reinterpret_cast<DB*>(db_);
  return pkgdepdb_strlist_del_r(db->assume_found_rules_, index, count);
}

void pkgdepdb_db_relink_all(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->RelinkAll();
}

void pkgdepdb_db_fix_paths(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->FixPaths();
}

pkgdepdb_bool pkgdepdb_db_wipe_packages(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->WipePackages();
}

pkgdepdb_bool pkgdepdb_db_wipe_filelists(pkgdepdb_db *db_) {
  auto db = reinterpret_cast<DB*>(db_);
  return db->WipeFilelists();
}

pkgdepdb_bool pkgdepdb_db_object_is_broken(pkgdepdb_db *db_, pkgdepdb_elf elf_)
{
  auto db = reinterpret_cast<DB*>(db_);
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  return db->IsBroken(elf) ? 1 : 0;
}

} /* extern "C" */
