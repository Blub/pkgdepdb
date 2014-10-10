#include "main.h"
#include "elf.h"
#include "package.h"
#include "db.h"

#include "pkgdepdb.h"

using namespace pkgdepdb;

extern "C" {

pkgdepdb_cfg *pkgdepdb_cfg_new() {
  return reinterpret_cast<pkgdepdb_cfg*>(new Config);
}

void pkgdepdb_cfg_delete(pkgdepdb_cfg *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  delete cfg;
}

pkgdepdb_bool pkgdepdb_cfg_load_default(pkgdepdb_cfg *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return cfg->ReadConfig();
}

pkgdepdb_bool pkgdepdb_cfg_load(pkgdepdb_cfg *cfg_, const char *filepath)
{
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return cfg->ReadConfig(filepath);
}

pkgdepdb_bool pkgdepdb_cfg_read(pkgdepdb_cfg *cfg_, const char *name,
                                const char *data, size_t length)
{
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return cfg->ReadConfig(name, data, length);
}

const char *pkgdepdb_cfg_database(pkgdepdb_cfg *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return cfg->database_.c_str();
}

void pkgdepdb_cfg_set_database(pkgdepdb_cfg *cfg_, const char *path) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  cfg->database_ = path;
}

unsigned int pkgdepdb_cfg_verbosity(pkgdepdb_cfg *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return cfg->verbosity_;
}

void pkgdepdb_cfg_set_verbosity(pkgdepdb_cfg *cfg_, unsigned int v) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  cfg->verbosity_ = v;
}

pkgdepdb_bool pkgdepdb_cfg_quiet(pkgdepdb_cfg *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return cfg->quiet_;
}

void pkgdepdb_cfg_set_quiet(pkgdepdb_cfg *cfg_, pkgdepdb_bool v) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  cfg->quiet_ = v;
}

pkgdepdb_bool pkgdepdb_cfg_package_depends(pkgdepdb_cfg *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return cfg->package_depends_;
}

void pkgdepdb_cfg_set_package_depends(pkgdepdb_cfg *cfg_,
                                         pkgdepdb_bool v)
{
  auto cfg = reinterpret_cast<Config*>(cfg_);
  cfg->package_depends_ = !!v;
}

pkgdepdb_bool pkgdepdb_cfg_package_file_lists(pkgdepdb_cfg *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return cfg->package_filelist_;
}

void pkgdepdb_cfg_set_package_file_lists(pkgdepdb_cfg *cfg_, pkgdepdb_bool v) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  cfg->package_filelist_ = !!v;
}

pkgdepdb_bool pkgdepdb_cfg_package_info(pkgdepdb_cfg *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return cfg->package_info_;
}

void pkgdepdb_cfg_set_package_info(pkgdepdb_cfg *cfg_, pkgdepdb_bool v) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  cfg->package_info_ = !!v;
}

unsigned int pkgdepdb_cfg_max_jobs(pkgdepdb_cfg *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return cfg->max_jobs_;
}

void pkgdepdb_cfg_set_max_jobs(pkgdepdb_cfg *cfg_, unsigned int v) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  cfg->max_jobs_ = v;
}

unsigned int pkgdepdb_cfg_log_level(pkgdepdb_cfg *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return cfg->log_level_;
}

void pkgdepdb_cfg_set_log_level(pkgdepdb_cfg *cfg_, unsigned int v) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  cfg->log_level_ = v;
}

unsigned int pkgdepdb_cfg_json(pkgdepdb_cfg *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return cfg->json_;
}

void pkgdepdb_cfg_set_json(pkgdepdb_cfg *cfg_, unsigned int v) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  cfg->json_ = v;
}

} // extern "C"
