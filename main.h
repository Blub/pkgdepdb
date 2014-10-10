#ifndef PKGDEPDB_MAIN_H__
#define PKGDEPDB_MAIN_H__

#include <stdint.h>

#include <memory>
#include <utility>
using ::std::move;
template<class T, class Deleter = ::std::default_delete<T>>
using uniq = ::std::unique_ptr<T,Deleter>;

#include <string>
using ::std::string;

#include <vector>
template<class T, class Alloc = ::std::allocator<T>>
using vec = ::std::vector<T,Alloc>;
using StringList = vec<string>;

#include <tuple>
using std::tuple;
using std::make_tuple;
using Depend     = tuple<string,string>;
using DependList = vec<Depend>;

#include <map>

#include <set>
using StringSet  = std::set<string>;

#include <functional>
using std::function;

#include "util.h"

#include "config.h"

namespace pkgdepdb {

typedef unsigned int uint;

struct Elf;
using ObjectSet   = std::set<rptr<Elf>>;
using ObjectList  = vec<rptr<Elf>>;
struct Package;
using PackageList = vec<Package*>;

void split_dependency(const string &full, string &dep, string &constraint);
#ifdef PKGDEPDB_ENABLE_ALPM
void split_constraint(const string &full, string &op, string &ver);
bool package_satisfies(const Package *other,
                       const string &dep,
                       const string &op,
                       const string &ver);
#endif

void fixpath    (string& path);
void fixpathlist(string& pathlist);

using PkgMap     = std::map<string, const Package*>;
using PkgListMap = std::map<string, vec<const Package*>>;
using ObjListMap = std::map<string, vec<const Elf*>>;

namespace filter {
class PackageFilter;
class ObjectFilter;
class StringFilter;
} // ::pkgdepdb::filter

using FilterList    = vec<uniq<filter::PackageFilter>>;
using ObjFilterList = vec<uniq<filter::ObjectFilter>>;
using StrFilterList = vec<uniq<filter::StringFilter>>;

namespace JSONBits {
  // NOTE: Keep in sync with pkgdepdb.h's PKGDEPDB_JSON_BITS
  static const unsigned int
    Query = (1<<0),
    DB    = (1<<1);
}

enum LogLevel {
  // NOTE: Keep in sync with pkgdepdb.h's PKGDEPDB_CONFIG_LOG_LEVEL
  Debug, Message, Print, Warn, Error
};

struct Config {
  string database_         = "";
  uint   verbosity_        = 0;
  bool   quiet_            = false;
  bool   package_depends_  = true;
  bool   package_filelist_ = true;
  bool   package_info_     = true;
  uint   json_             = 0;
  uint   max_jobs_         = 0;
  uint   log_level_        = LogLevel::Message;

  Config();
  Config(Config&&) = delete;
  Config(const Config&) = delete;
  ~Config();

  bool ReadConfig();
  bool ReadConfig(const char *path);
  void Log(uint level, const char *msg, ...) const;

  static const char *ParseJSONBit(const char *bit, uint &opt_json);
  static bool str2bool(const string&);

 private:
  bool ReadConfig(std::istream&, const char *path);
};

} // ::pkgdepdb

#endif
