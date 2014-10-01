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

#include <map>

#include <set>
using StringSet  = std::set<string>;

#include <functional>
using std::function;

#include "util.h"

#include "config.h"

namespace pkgdepdb {

typedef unsigned int uint;

enum LogLevel {
  Debug, Message, Print, Warn, Error
};
void log(int level, const char *msg, ...);

struct Elf;
using ObjectSet   = std::set<rptr<Elf>>;
using ObjectList  = vec<rptr<Elf>>;
struct Package;
using PackageList = vec<Package*>;

#ifdef PKGDEPDB_WITH_ALPM
bool split_depstring  (const string &str, string &name, string &op, string &v);
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

} // ::pkgdepdb

#endif
