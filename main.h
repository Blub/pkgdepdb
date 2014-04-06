#ifndef WDEPTRACK_MAIN_H__
#define WDEPTRACK_MAIN_H__

#include <stdint.h>

#include <memory>
#include <vector>
#include <map>
#include <set>
#include <functional>

#include "util.h"

#ifndef PKGDEPDB_V_MAJ
# error "PKGDEPDB_V_MAJ not defined"
#endif
#ifndef PKGDEPDB_V_MIN
# error "PKGDEPDB_V_MIN not defined"
#endif
#ifndef PKGDEPDB_V_PAT
# error "PKGDEPDB_V_PAT not defined"
#endif

#ifndef PKGDEPDB_ETC
# error "No PKGDEPDB_ETC defined"
#endif

#define RPKG_IND_STRING2(x) #x
#define RPKG_IND_STRING(x) RPKG_IND_STRING2(x)
#define VERSION_STRING \
  RPKG_IND_STRING(PKGDEPDB_V_MAJ) "." \
  RPKG_IND_STRING(PKGDEPDB_V_MIN) "." \
  RPKG_IND_STRING(PKGDEPDB_V_PAT)

#ifdef GITINFO
# define FULL_VERSION_STRING \
    VERSION_STRING "-git: " GITINFO
#else
# define FULL_VERSION_STRING VERSION_STRING
#endif

enum {
  Debug,
  Message, // color ftw
  Print,   // regular printing
  Warn,
  Error
};

void log(int level, const char *msg, ...);

extern std::string  opt_default_db;
extern unsigned int opt_verbosity;
extern bool         opt_quiet;
extern bool         opt_package_depends;
extern bool         opt_package_filelist;
extern unsigned int opt_max_jobs;
extern unsigned int opt_json;

namespace JSONBits {
  static const unsigned int
    Query = (1<<0),
    DB    = (1<<1);
}

bool ReadConfig     ();
bool CfgStrToBool   (const std::string& line);
bool CfgParseJSONBit(const char *bit);

class Package;
class Elf;

using PackageList = std::vector<Package*>;
using ObjectList  = std::vector<rptr<Elf>>;
using StringList  = std::vector<std::string>;

using ObjectSet   = std::set<rptr<Elf>>;
using StringSet   = std::set<std::string>;

class Elf {
public:
  Elf();
  Elf(const Elf& cp);
  static Elf* Open(const char* data, size_t size, bool *err, const char *name);

public:
  size_t refcount_;

  // path + name separated
  std::string dirname_;
  std::string basename_;

  // classification:
  unsigned char ei_class_; // 32/64 bit
  unsigned char ei_data_;  // endianess
  unsigned char ei_osabi_; // freebsd/linux/...

  // requirements:
  bool                     rpath_set_;
  bool                     runpath_set_;
  std::string              rpath_;
  std::string              runpath_;
  std::vector<std::string> needed_;

public: // utility functions while loading
  void solve_paths(const std::string& origin);
  bool can_use(const Elf &other, bool strict) const;

public: // utility functions for printing stuff
  const char *classString() const;
  const char *dataString()  const;
  const char *osabiString() const;

public: // NOT serialized INSIDE the object, but as part of the DB
        // (for compatibility with older database dumps)
  ObjectSet req_found_;
  StringSet req_missing_;

public: // NOT SERIALIZED:
  struct {
    size_t id;
  } json_;

  Package *owner_;
};

/// Package class
/// reads a package archive, extracts information
#ifdef WITH_ALPM
bool split_depstring(const std::string &str,
                     std::string &name, std::string &op, std::string &ver);
bool
package_satisfies(const Package *other,
                  const std::string &dep,
                  const std::string &op,
                  const std::string &ver);
#endif
class Package {
public:
  static Package* Open(const std::string& path);

  std::string             name_;
  std::string             version_;
  std::vector<rptr<Elf> > objects_;

  // DB version 3:
  StringList              depends_;
  StringList              optdepends_;
  StringList              provides_;
  StringList              conflicts_;
  StringList              replaces_;
  // DB version 5:
  StringSet               groups_;
  // DB version 6:
  // the filelist includes object files in v6 - makes things easier
  StringList              filelist_;

  void show_needed();
  Elf* find(const std::string &dirname, const std::string &basename) const;

public: // loading utiltiy functions
  void guess(const std::string& name);
  bool conflicts_with(const Package&) const;

public: // NOT SERIALIZED:
  // used only while loading an archive
  struct {
    std::map<std::string, std::string> symlinks;
  } load_;
};

void fixpath(std::string& path);
void fixpathlist(std::string& pathlist);

using PkgMap     = std::map<std::string, const Package*>;
using PkgListMap = std::map<std::string, std::vector<const Package*>>;
using ObjListMap = std::map<std::string, std::vector<const Elf*>>;

namespace filter {
class PackageFilter;
class ObjectFilter;
class StringFilter;
}
using FilterList = std::vector<std::unique_ptr<filter::PackageFilter>>;
using ObjFilterList = std::vector<std::unique_ptr<filter::ObjectFilter>>;
using StrFilterList = std::vector<std::unique_ptr<filter::StringFilter>>;

class DB {
public:
  static uint16_t CURRENT;

  using IgnoreMissingRule = struct {
    std::string package;
    std::string object;
    std::string what;
  };

  DB();
  ~DB();
  DB(bool wiped, const DB& copy);

  uint16_t    loaded_version_;
  bool        strict_linking_; // stored as flag bit

  std::string name_;
  StringList  library_path_;

  PackageList packages_;
  ObjectList  objects_;

  StringSet                         ignore_file_rules_;
  std::map<std::string, StringList> package_library_path_;
  StringSet                         base_packages_;
  StringSet                         assume_found_rules_;

public:
  bool install_package(Package* &&pkg);
  bool delete_package (const std::string& name);
  Elf *find_for       (const Elf*, const std::string& lib, const StringList *extrapath) const;
  void link_object    (Elf*, const Package *owner, ObjectSet &req_found, StringSet &req_missing) const;
  void link_object_do (Elf*, const Package *owner);
  void relink_all     ();
  void fix_paths      ();
  bool wipe_packages  ();
  bool wipe_filelists ();

#ifdef ENABLE_THREADS
  void relink_all_threaded();
#endif

  Package* find_pkg   (const std::string& name) const;
  PackageList::const_iterator
           find_pkg_i (const std::string& name) const;

  void show_info();
  void show_info_json();
  void show_packages(bool flt_broken, bool flt_notempty, const FilterList&, const ObjFilterList&);
  void show_packages_json(bool flt_broken, bool flt_notempty, const FilterList&, const ObjFilterList&);
  void show_objects(const FilterList&, const ObjFilterList&);
  void show_objects_json(const FilterList&, const ObjFilterList&);
  void show_missing();
  void show_missing_json();
  void show_found();
  void show_found_json();
  void show_filelist(const FilterList&, const StrFilterList&);
  void show_filelist_json(const FilterList&, const StrFilterList&);
  void check_integrity(const FilterList &pkg_filters,
                       const ObjFilterList &obj_filters) const;
  void check_integrity(const Package    *pkg,
                       const PkgMap     &pkgmap,
                       const PkgListMap &providemap,
                       const PkgListMap &replacemap,
                       const PkgMap     &basemap,
                       const ObjListMap &objmap,
                       const std::vector<const Package*> &base,
                       const ObjFilterList &obj_filters) const;

  bool store(const std::string& filename);
  bool read (const std::string& filename);
  bool empty() const;

  bool ld_append (const std::string& dir);
  bool ld_prepend(const std::string& dir);
  bool ld_delete (const std::string& dir);
  bool ld_delete (size_t i);
  bool ld_insert (const std::string& dir, size_t i);
  bool ld_clear  ();

  bool ignore_file  (const std::string& name);
  bool unignore_file(const std::string& name);
  bool unignore_file(size_t id);

  bool assume_found  (const std::string& name);
  bool unassume_found(const std::string& name);
  bool unassume_found(size_t id);

  bool add_base_package   (const std::string& name);
  bool remove_base_package(const std::string& name);
  bool remove_base_package(size_t id);

  bool pkg_ld_insert(const std::string& package, const std::string& path, size_t i);
  bool pkg_ld_delete(const std::string& package, const std::string& path);
  bool pkg_ld_delete(const std::string& package, size_t i);
  bool pkg_ld_clear (const std::string& package);

private:
  bool elf_finds(const Elf*, const std::string& lib, const StringList *extrapath) const;

  const StringList* get_obj_libpath(const Elf*) const;
  const StringList* get_pkg_libpath(const Package*) const;

public:
  bool is_broken(const Package *pkg) const;
  bool is_broken(const Elf *elf) const;
  bool is_empty (const Package *elf, const ObjFilterList &filters) const;

public: // NOT SERIALIZED:
  bool contains_package_depends_;
  bool contains_groups_;
  bool contains_filelists_;
};

namespace filter {
class Match {
public:
  size_t refcount_; // make it a capturable rptr
  Match();
  virtual ~Match();
  virtual bool operator()(const std::string&) const = 0;

  static rptr<Match> CreateExact(std::string &&text);
  static rptr<Match> CreateGlob (std::string &&text);
#ifdef WITH_REGEX
  static rptr<Match> CreateRegex(std::string &&text, bool icase);
#endif
};

class PackageFilter {
protected:
  PackageFilter(bool);
public:
  PackageFilter() = delete;

  bool negate_;
  virtual ~PackageFilter();
  virtual bool visible(const Package &pkg) const {
    (void)pkg; return true;
  }
  virtual bool visible(const DB& db, const Package &pkg) const {
    (void)db; return visible(pkg);
  }
  inline bool operator()(const DB& db, const Package &pkg) const {
    return visible(db, pkg) != negate_;
  }

  static unique_ptr<PackageFilter> name         (rptr<Match>, bool neg);
  static unique_ptr<PackageFilter> group        (rptr<Match>, bool neg);
  static unique_ptr<PackageFilter> depends      (rptr<Match>, bool neg);
  static unique_ptr<PackageFilter> optdepends   (rptr<Match>, bool neg);
  static unique_ptr<PackageFilter> alldepends   (rptr<Match>, bool neg);
  static unique_ptr<PackageFilter> provides     (rptr<Match>, bool neg);
  static unique_ptr<PackageFilter> conflicts    (rptr<Match>, bool neg);
  static unique_ptr<PackageFilter> replaces     (rptr<Match>, bool neg);
  static unique_ptr<PackageFilter> pkglibdepends(rptr<Match>, bool neg);

  static unique_ptr<PackageFilter> broken       (bool neg);
};

class ObjectFilter {
protected:
  ObjectFilter(bool);
public:
  ObjectFilter() = delete;

  size_t refcount_;
  bool   negate_;

  virtual ~ObjectFilter();
  virtual bool visible(const Elf &elf) const {
    (void)elf; return true;
  }
  virtual bool visible(const DB& db, const Elf &elf) const {
    (void)db; return visible(elf);
  }
  inline bool operator()(const DB& db, const Elf &elf) const {
    return visible(db, elf) != negate_;
  }

  static unique_ptr<ObjectFilter> name   (rptr<Match>, bool neg);
  static unique_ptr<ObjectFilter> path   (rptr<Match>, bool neg);
  static unique_ptr<ObjectFilter> depends(rptr<Match>, bool neg);
};

class StringFilter {
protected:
  StringFilter(bool);
public:
  StringFilter() = delete;

  bool negate_;
  virtual ~StringFilter();

  virtual bool visible(const std::string& str) const {
    (void)str; return true;
  }
  inline bool operator()(const std::string &str) const {
    return visible(str) != negate_;
  }

  static unique_ptr<StringFilter> filter(rptr<Match>, bool neg);
};

}

bool db_store_json(DB *db, const std::string& filename);

#endif
