#include "main.h"
#include "elf.h"
#include "package.h"
#include "db.h"

#include "pkgdepdb.h"

using namespace pkgdepdb;

extern "C" {

pkgdepdb_config *pkgdepdb_config_new() {
  return reinterpret_cast<pkgdepdb_config*>(new Config);
}

void pkgdepdb_config_delete(pkgdepdb_config *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  delete cfg;
}

int pkgdepdb_config_load_default(pkgdepdb_config *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return cfg->ReadConfig();
}

int pkgdepdb_config_load(pkgdepdb_config *cfg_, const char *filepath) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return cfg->ReadConfig(filepath);
}

const char *pkgdepdb_config_database(pkgdepdb_config *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return cfg->database_.c_str();
}

void pkgdepdb_config_set_database(pkgdepdb_config *cfg_, const char *path) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  cfg->database_ = path;
}

unsigned int pkgdepdb_config_verbosity(pkgdepdb_config *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return cfg->verbosity_;
}

void pkgdepdb_config_set_verbosity(pkgdepdb_config *cfg_, unsigned int v) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  cfg->verbosity_ = v;
}

int pkgdepdb_config_quiet(pkgdepdb_config *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return cfg->quiet_;
}

void pkgdepdb_config_set_quiet(pkgdepdb_config *cfg_, int v) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  cfg->quiet_ = v;
}

int pkgdepdb_config_package_depends(pkgdepdb_config *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return cfg->package_depends_;
}

void pkgdepdb_config_set_package_depends(pkgdepdb_config *cfg_, int v) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  cfg->package_depends_ = !!v;
}

int pkgdepdb_config_package_file_lists(pkgdepdb_config *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return cfg->package_filelist_;
}

void pkgdepdb_config_set_package_file_lists(pkgdepdb_config *cfg_, int v) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  cfg->package_filelist_ = !!v;
}

unsigned int pkgdepdb_config_max_jobs(pkgdepdb_config *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return cfg->max_jobs_;
}

void pkgdepdb_config_set_max_jobs(pkgdepdb_config *cfg_, unsigned int v) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  cfg->max_jobs_ = v;
}

} // extern "C"
