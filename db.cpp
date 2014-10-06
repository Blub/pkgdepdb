#include <memory>
#include <algorithm>
#include <utility>

#include "main.h"

#ifdef PKGDEPDB_ENABLE_THREADS
#  include <atomic>
#  include <thread>
#  include <future>
#  include <unistd.h>
#endif

#ifdef PKGDEPDB_ENABLE_ALPM
#  include <alpm.h>
#endif

#include "elf.h"
#include "package.h"
#include "db.h"
#include "filter.h"

namespace pkgdepdb {

string strref::empty("");

DB::DB(const Config& optconfig)
: config_(optconfig) {
  loaded_version_           = DB::CURRENT;
  contains_package_depends_ = false;
  contains_make_depends_    = false;
  contains_groups_          = false;
  contains_filelists_       = false;
  strict_linking_           = false;
}

DB::~DB() {
  for (auto &pkg : packages_)
    delete pkg;
}

template<typename T>
static void stdreplace(T &what, const T &with) {
  what.~T();
  new (&what) T(with);
}

DB::DB(bool wiped, const DB &copy)
: name_                (copy.name_),
  library_path_        (copy.library_path_),
  ignore_file_rules_   (copy.ignore_file_rules_),
  package_library_path_(copy.package_library_path_),
  base_packages_       (copy.base_packages_),
  config_              (copy.config_)
{
  loaded_version_ = copy.loaded_version_;
  strict_linking_ = copy.strict_linking_;
  if (!wiped) {
    stdreplace(packages_, copy.packages_);
    stdreplace(objects_,  copy.objects_);
  }
}

PackageList::const_iterator DB::FindPkg_i(const string& name) const {
  return std::find_if(packages_.begin(), packages_.end(),
    [&name](const Package *pkg) { return pkg->name_ == name; });
}

Package* DB::FindPkg(const string& name) const {
  auto pkg = FindPkg_i(name);
  return (pkg != packages_.end()) ? *pkg : nullptr;
}

bool DB::WipePackages() {
  if (Empty())
    return false;
  objects_.clear();
  packages_.clear();
  return true;
}

bool DB::WipeFilelists() {
  bool hadfiles = contains_filelists_;
  for (auto &pkg : packages_) {
    if (!pkg->filelist_.empty()) {
      pkg->filelist_.clear();
      hadfiles = true;
    }
  }
  contains_filelists_ = false;
  return hadfiles;
}

const StringList* DB::GetObjectLibPath(const Elf *elf) const {
  return elf->owner_ ? GetPackageLibPath(elf->owner_) : nullptr;
}

const StringList* DB::GetPackageLibPath(const Package *pkg) const {
  if (!package_library_path_.size())
    return nullptr;

  auto iter = package_library_path_.find(pkg->name_);
  if (iter != package_library_path_.end())
    return &iter->second;
  return nullptr;
}

bool DB::DeletePackage(const string& name)
{
  const Package *old; {
    auto pkgiter = FindPkg_i(name);
    if (pkgiter == packages_.end())
      return true;

    old = *pkgiter;
    packages_.erase(packages_.begin() + (pkgiter - packages_.begin()));
  }

  for (auto &elfsp : old->objects_) {
    Elf *elf = elfsp.get();
    // remove the object from the list
    objects_.erase(std::remove(objects_.begin(), objects_.end(), elf),
                   objects_.end());
  }

  for (auto &seeker : objects_) {
    for (auto &elfsp : old->objects_) {
      Elf *elf = elfsp.get();
      // for each object which depends on this object,
      // search for a replacing object
      auto ref = std::find(seeker->req_found_.begin(),
                           seeker->req_found_.end(),
                           elf);
      if (ref == seeker->req_found_.end())
        continue;
      seeker->req_found_.erase(ref);

      const StringList *libpaths = GetObjectLibPath(seeker);
      if (Elf *other = FindFor (seeker, elf->basename_, libpaths))
        seeker->req_found_.insert(other);
      else
        seeker->req_missing_.insert(elf->basename_);
    }
  }

  delete old;

  objects_.erase(
    std::remove_if(objects_.begin(), objects_.end(),
      [](rptr<Elf> &obj) { return 1 == obj->refcount_; }),
    objects_.end());

  return true;
}

static bool pathlist_contains(const string& list, const string& path) {
  size_t at = 0;
  size_t to = list.find_first_of(':', 0);
  while (to != string::npos) {
    if (list.compare(at, to-at, path) == 0)
      return true;
    at = to+1;
    to = list.find_first_of(':', at);
  }
  if (list.compare(at, string::npos, path) == 0)
    return true;
  return false;
}

bool DB::ElfFinds(const Elf *elf, const string& path,
                  const StringList *extrapaths) const
{
  // DT_RPATH first
  if (elf->rpath_set_ && pathlist_contains(elf->rpath_, path))
    return true;

  // LD_LIBRARY_PATH - ignored

  // DT_RUNPATH
  if (elf->runpath_set_ && pathlist_contains(elf->runpath_, path))
    return true;

  // Trusted Paths
  if (path == "/lib" ||
      path == "/usr/lib")
  {
    return true;
  }

  if (std::find(library_path_.begin(), library_path_.end(), path)
      != library_path_.end())
  {
    return true;
  }

  if (extrapaths) {
    if (std::find(extrapaths->begin(), extrapaths->end(), path)
        != extrapaths->end())
    {
      return true;
    }
  }

  return false;
}

bool DB::InstallPackage(Package* &&pkg) {
  if (!DeletePackage(pkg->name_))
    return false;

  packages_.push_back(pkg);
  if (!pkg->depends_.empty()    ||
      !pkg->optdepends_.empty() ||
      !pkg->replaces_.empty()   ||
      !pkg->conflicts_.empty()  ||
      !pkg->provides_.empty())
  {
    contains_package_depends_ = true;
  }
  if (!pkg->makedepends_.empty())
    contains_make_depends_ = true;
  if (pkg->groups_.size())
    contains_groups_ = true;
  if (pkg->filelist_.size())
    contains_filelists_ = true;

  const StringList *libpaths = GetPackageLibPath(pkg);

  for (auto &obj : pkg->objects_)
    objects_.push_back(obj);
  // loop anew since we need to also be able to found our own packages
  for (auto &obj : pkg->objects_)
    LinkObject_do(obj, pkg);

  // check for packages which are looking for any of our packages
  for (auto &seeker : objects_) {
    for (auto &obj : pkg->objects_) {
      if (!seeker->CanUse(*obj, strict_linking_) ||
          !ElfFinds(seeker, obj->dirname_, libpaths))
      {
        continue;
      }

      if (0 != seeker->req_missing_.erase(obj->basename_))
        seeker->req_found_.insert(obj);
    }
  }
  return true;
}

Elf* DB::FindFor(const Elf *obj, const string& needed,
                 const StringList *extrapath) const
{
  config_.Log(Debug, "dependency of %s/%s   :  %s\n",
              obj->dirname_.c_str(), obj->basename_.c_str(), needed.c_str());
  for (auto &lib : objects_) {
    if (!obj->CanUse(*lib, strict_linking_)) {
      config_.Log(Debug, "  skipping %s/%s (objclass)\n",
                  lib->dirname_.c_str(), lib->basename_.c_str());
      continue;
    }
    if (lib->basename_    != needed) {
      config_.Log(Debug, "  skipping %s/%s (wrong name)\n",
                  lib->dirname_.c_str(), lib->basename_.c_str());
      continue;
    }
    if (!ElfFinds(obj, lib->dirname_, extrapath)) {
      config_.Log(Debug, "  skipping %s/%s (not visible)\n",
                  lib->dirname_.c_str(), lib->basename_.c_str());
      continue;
    }
    // same class, same name, and visible...
    return lib;
  }
  return 0;
}

void DB::LinkObject_do(Elf *obj, const Package *owner) {
  obj->req_found_.clear();
  obj->req_missing_.clear();
  LinkObject(obj, owner, obj->req_found_, obj->req_missing_);
}

void DB::LinkObject(Elf *obj, const Package *owner,
                    ObjectSet &req_found, StringSet &req_missing) const
{
  if (ignore_file_rules_.size()) {
    string full = obj->dirname_ + "/" + obj->basename_;
    if (ignore_file_rules_.find(full) != ignore_file_rules_.end())
      return;
  }

  const StringList *libpaths = GetPackageLibPath(owner);

  for (auto &needed : obj->needed_) {
    Elf *found = FindFor (obj, needed, libpaths);
    if (found)
      req_found.insert(found);
    else if (assume_found_rules_.find(needed) == assume_found_rules_.end())
      req_missing.insert(needed);
  }
}

#ifdef PKGDEPDB_ENABLE_THREADS
namespace thread {

  static unsigned int ncpus_init() {
    long v = sysconf(_SC_NPROCESSORS_CONF);
    return (v <= 0 ? 1 : (unsigned int)v);
  }

  static unsigned int ncpus = ncpus_init();

  using status_printer_func_t =
    void (unsigned long at, unsigned long count, unsigned long threads);

  template<typename PerThread>
  using worker_func_t =
    void(std::atomic_ulong*, size_t from, size_t to, PerThread&);

  template<typename PerThread>
  using merger_func_t = void(vec<PerThread>&&);

  template<typename PerThread>
  void work(unsigned long                      Count,
            function<status_printer_func_t>    StatusPrinter,
            function<worker_func_t<PerThread>> Worker,
            function<merger_func_t<PerThread>> Merger,
            const Config&                      Config)
  {
    unsigned long threadcount = thread::ncpus;
    if (Config.max_jobs_ >= 1 && Config.max_jobs_ < threadcount)
      threadcount = Config.max_jobs_;

    unsigned long  obj_per_thread = Count / threadcount;
    if (!Config.quiet_)
      StatusPrinter(0, Count, threadcount);

    if (threadcount == 1) {
      for (unsigned long i = 0; i != Count; ++i) {
        PerThread Data;
        Worker(nullptr, i, i+1, Data);
        if (!Config.quiet_)
          StatusPrinter(i, Count, 1);
      }
      return;
    }

    // data created by threads, to be merged in the merger
    vec<PerThread> Data;
    Data.resize(threadcount);

    std::atomic_ulong         counter(0);
    vec<std::thread*> threads;

    unsigned long i;
    for (i = 0; i != threadcount-1; ++i) {
      threads.emplace_back(
        new std::thread(Worker,
                        &counter,
                        i*obj_per_thread,
                        i*obj_per_thread + obj_per_thread,
                        std::ref(Data[i])));
    }
    threads.emplace_back(
      new std::thread(Worker,
                      &counter,
                      i*obj_per_thread, Count,
                      std::ref(Data[i])));
    if (!Config.quiet_) {
      unsigned long c = 0;
      while (c != Count) {
        c = counter.load();
        StatusPrinter(c, Count, threadcount);
        usleep(100000);
      }
    }

    for (i = 0; i != threadcount; ++i) {
      threads[i]->join();
      delete threads[i];
    }
    Merger(move(Data));
    if (!Config.quiet_)
      StatusPrinter(Count, Count, threadcount);
  }

} // namespace thread

void DB::RelinkAll_Threaded() {
  //using FoundMap   = std::map<Elf*, ObjectSet>;
  //using MissingMap = std::map<Elf*, StringSet>;
  //using Tuple      = std::tuple<FoundMap, MissingMap>;
  auto worker = [this](std::atomic_ulong *count, size_t from, size_t to, int&)
  {
    //FoundMap   *f = &std::get<0>(tup);
    //MissingMap *m = &std::get<1>(tup);
    for (size_t i = from; i != to; ++i) {
      const Package *pkg = this->packages_[i];

      for (auto &obj : pkg->objects_) {
        this->LinkObject_do(obj, pkg);
        //ObjectSet req_found;
        //StringSet req_missing;
        //this->LinkObject(obj, pkg, req_found, req_missing);
        //(*f)[obj] = move(req_found);
        //(*m)[obj] = move(req_missing);
      }

      if (count && !config_.quiet_)
        (*count)++;
    }
  };
  auto merger = [this](vec<int> &&) {
    //for (auto &t : tup) {
    //  FoundMap   &found = std::get<0>(t);
    //  MissingMap &missing = std::get<1>(t);
    //  for (auto &f : found)
    //    f.first->req_found = move(f.second);
    //  for (auto &m : missing)
    //    m.first->req_missing = move(m.second);
    //}
  };
  double fac = 100.0 / double(packages_.size());
  unsigned int pc = 1000;
  auto status = [fac, &pc](unsigned long at, unsigned long cnt,
                           unsigned long threadcount)
  {
    auto newpc = (unsigned int)(fac * double(at));
    if (newpc == pc)
      return;
    pc = newpc;
    printf("\rrelinking: %3u%% (%lu / %lu packages) [%lu]",
           pc, at, cnt, threadcount);
    fflush(stdout);
    if (at == cnt)
      printf("\n");
  };
  thread::work<int>(packages_.size(), status, worker, merger, config_);
}
#endif

void DB::RelinkAll() {
  if (!packages_.size())
    return;

#ifdef PKGDEPDB_ENABLE_THREADS
  if (config_.max_jobs_ != 1   &&
      thread::ncpus     >  1   &&
      packages_.size()  >  100 &&
      objects_.size()   >= 300)
  {
    return RelinkAll_Threaded();
  }
#endif

  unsigned long pkgcount = packages_.size();
  double        fac   = 100.0 / double(pkgcount);
  unsigned long count = 0;
  unsigned int  pc    = 0;
  if (!config_.quiet_) {
    printf("relinking: 0%% (0 / %lu packages)", pkgcount);
    fflush(stdout);
  }
  for (auto &pkg : packages_) {
    for (auto &obj : pkg->objects_) {
      LinkObject_do(obj, pkg);
    }
    if (!config_.quiet_) {
      ++count;
      auto newpc = (unsigned int)(fac * double(count));
      if (newpc != pc) {
        pc = newpc;
        printf("\rrelinking: %3u%% (%lu / %lu packages)",
               pc, count, pkgcount);
        fflush(stdout);
      }
    }
  }
  if (!config_.quiet_) {
    printf("\rrelinking: 100%% (%lu / %lu packages)\n",
           count, pkgcount);
  }
}

void DB::FixPaths() {
  for (auto &obj : objects_) {
    fixpathlist(obj->rpath_);
    fixpathlist(obj->runpath_);
  }
}

bool DB::Empty() const {
  return packages_.size() == 0 &&
         objects_.size()  == 0;
}

bool DB::LD_Clear() {
  if (library_path_.size()) {
    library_path_.clear();
    return true;
  }
  return false;
}

static string fixcpath(const string& dir) {
  string s(dir);
  fixpath(s);
  return move(dir);
}

bool DB::LD_Append(const string& dir) {
  return LD_Insert(fixcpath(dir), library_path_.size());
}

bool DB::LD_Prepend(const string& dir) {
  return LD_Insert(fixcpath(dir), 0);
}

bool DB::LD_Delete(size_t i) {
  if (!library_path_.size() || i >= library_path_.size())
    return false;
  library_path_.erase(library_path_.begin() + i);
  return true;
}

bool DB::LD_Delete(const string& dir_) {
  if (!dir_.length())
    return false;
  if (dir_[0] >= '0' && dir_[0] <= '9') {
    return LD_Delete(strtoul(dir_.c_str(), nullptr, 0));
  }
  string dir(dir_);
  fixpath(dir);
  auto old = std::find(library_path_.begin(), library_path_.end(), dir);
  if (old != library_path_.end()) {
    library_path_.erase(old);
    return true;
  }
  return false;
}

bool DB::LD_Insert(const string& dir_, size_t i) {
  string dir(dir_);
  fixpath(dir);
  if (i > library_path_.size())
    i = library_path_.size();

  auto old = std::find(library_path_.begin(), library_path_.end(), dir);
  if (old == library_path_.end()) {
    library_path_.insert(library_path_.begin() + i, dir);
    return true;
  }
  size_t oldidx = old - library_path_.begin();
  if (oldidx == i)
    return false;
  // exists
  library_path_.erase(old);
  library_path_.insert(library_path_.begin() + i, dir);
  return true;
}

bool DB::PKG_LD_Insert(const string& package,
                       const string& directory,
                       size_t        i)
{
  string dir(directory);
  fixpath(dir);
  StringList &path(package_library_path_[package]);

  if (i > path.size())
    i = path.size();

  auto old = std::find(path.begin(), path.end(), dir);
  if (old == path.end()) {
    path.insert(path.begin() + i, dir);
    return true;
  }
  size_t oldidx = old - path.begin();
  if (oldidx == i)
    return false;
  // exists
  path.erase(old);
  path.insert(path.begin() + i, dir);
  return true;
}

bool DB::PKG_LD_Delete(const string& package, const string& directory) {
  string dir(directory);
  fixpath(dir);
  auto iter = package_library_path_.find(package);
  if (iter == package_library_path_.end())
    return false;

  StringList &path(iter->second);
  auto old = std::find(path.begin(), path.end(), dir);
  if (old != path.end()) {
    path.erase(old);
    if (!path.size())
      package_library_path_.erase(iter);
    return true;
  }
  return false;
}

bool DB::PKG_LD_Delete(const string& package, size_t i) {
  auto iter = package_library_path_.find(package);
  if (iter == package_library_path_.end())
    return false;

  StringList &path(iter->second);
  if (i >= path.size())
    return false;
  path.erase(path.begin()+i);
  if (!path.size())
    package_library_path_.erase(iter);
  return true;
}

bool DB::PKG_LD_Clear(const string& package) {
  auto iter = package_library_path_.find(package);
  if (iter == package_library_path_.end())
    return false;

  package_library_path_.erase(iter);
  return true;
}

bool DB::IgnoreFile_Add(const string& filename) {
  return std::get<1>(ignore_file_rules_.insert(fixcpath(filename)));
}

bool DB::IgnoreFile_Delete(const string& filename) {
  return (ignore_file_rules_.erase(fixcpath(filename)) > 0);
}

bool DB::IgnoreFile_Delete(size_t id) {
  if (id >= ignore_file_rules_.size())
    return false;
  auto iter = ignore_file_rules_.begin();
  while (id) {
    ++iter;
    --id;
  }
  ignore_file_rules_.erase(iter);
  return true;
}

bool DB::AssumeFound_Add(const string& name) {
  return std::get<1>(assume_found_rules_.insert(name));
}

bool DB::AssumeFound_Delete(const string& name) {
  return (assume_found_rules_.erase(name) > 0);
}

bool DB::AssumeFound_Delete(size_t id) {
  if (id >= assume_found_rules_.size())
    return false;
  auto iter = assume_found_rules_.begin();
  while (id) {
    ++iter;
    --id;
  }
  assume_found_rules_.erase(iter);
  return true;
}

bool DB::BasePackages_Add(const string& name) {
  return std::get<1>(base_packages_.insert(name));
}

bool DB::BasePackages_Delete(const string& name) {
  return (base_packages_.erase(name) > 0);
}

bool DB::BasePackages_Delete(size_t id) {
  if (id >= base_packages_.size())
    return false;
  auto iter = base_packages_.begin();
  while (id) {
    ++iter;
    --id;
  }
  base_packages_.erase(iter);
  return true;
}

void DB::ShowInfo() {
  if (config_.json_ & JSONBits::Query)
    return ShowInfo_json();

  printf("DB version: %u\n", loaded_version_);
  printf("DB name:    [%s]\n", name_.c_str());
  printf("DB flags:   { %s }\n",
         (strict_linking_ ? "strict" : "non_strict"));
  printf("Additional Library Paths:\n");
  unsigned id = 0;
  for (auto &p : library_path_)
    printf("  %u: %s\n", id++, p.c_str());
  if (ignore_file_rules_.size()) {
    printf("Ignoring the following files:\n");
    id = 0;
    for (auto &ign : ignore_file_rules_)
      printf("  %u: %s\n", id++, ign.c_str());
  }
  if (assume_found_rules_.size()) {
    printf("Assuming the following libraries to exist:\n");
    id = 0;
    for (auto &ign : assume_found_rules_)
      printf("  %u: %s\n", id++, ign.c_str());
  }
  if (package_library_path_.size()) {
    printf("Package-specific library paths:\n");
    id = 0;
    for (auto &iter : package_library_path_) {
      printf("  %s:\n", iter.first.c_str());
      id = 0;
      for (auto &path : iter.second)
        printf("    %u: %s\n", id++, path.c_str());
    }
  }
  if (base_packages_.size()) {
    printf("The following packages are base packages:\n");
    id = 0;
    for (auto &p : base_packages_)
      printf("  %u: %s\n", id++, p.c_str());
  }
}

bool DB::IsBroken(const Elf *obj) const {
  return obj->req_missing_.size() != 0;
}

bool DB::IsEmpty(const Package *pkg, const ObjFilterList &filters) const {
  size_t vis = 0;
  for (auto &obj : pkg->objects_) {
    if (util::all(filters, *this, *obj))
      ++vis;
  }
  return vis == 0;
}

bool DB::IsBroken(const Package *pkg) const {
  for (auto &obj : pkg->objects_) {
    if (IsBroken(obj))
      return true;
  }
  return false;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wno-format-nonliteral"
static void ShowDependList(const char *fmt, const DependList& lst) {
  for (const auto &dep : lst)
    printf(fmt, std::get<0>(dep).c_str(), std::get<1>(dep).c_str());
}
#pragma clang diagnostic pop

void DB::ShowPackages(bool                 filter_broken,
                      bool                 filter_notempty,
                      const FilterList    &pkg_filters,
                      const ObjFilterList &obj_filters)
{
  if (config_.json_ & JSONBits::Query)
    return ShowPackages_json(filter_broken, filter_notempty,
                             pkg_filters, obj_filters);

  if (!config_.quiet_)
    printf("Packages:%s\n", (filter_broken ? " (filter: 'broken')" : ""));
  for (auto &pkg : packages_) {
    if (!util::all(pkg_filters, *this, *pkg))
      continue;
    if (filter_broken && !IsBroken(pkg))
      continue;
    if (filter_notempty && IsEmpty(pkg, obj_filters))
      continue;
    if (config_.quiet_)
      printf("%s\n", pkg->name_.c_str());
    else
      printf("  -> %s - %s\n", pkg->name_.c_str(), pkg->version_.c_str());
    if (config_.verbosity_ >= 1) {
      for (auto &grp : pkg->groups_)
        printf("    is in group: %s\n", grp.c_str());
      ShowDependList("    depends on: %s\n",                pkg->depends_);
      ShowDependList("    depends optionally on: %s\n",     pkg->optdepends_);
      ShowDependList("    depends at compiletime on: %s\n", pkg->makedepends_);
      ShowDependList("    provides: %s\n",                  pkg->provides_);
      ShowDependList("    replaces: %s\n",                  pkg->replaces_);
      ShowDependList("    conflicts with: %s\n",            pkg->conflicts_);
      if (filter_broken) {
        for (auto &obj : pkg->objects_) {
          if (!util::all(obj_filters, *this, *obj))
            continue;
          if (IsBroken(obj)) {
            printf("    broken: %s / %s\n",
                   obj->dirname_.c_str(), obj->basename_.c_str());
            if (config_.verbosity_ >= 2) {
              for (auto &missing : obj->req_missing_)
                printf("      misses: %s\n", missing.c_str());
            }
          }
        }
      }
      else {
        for (auto &obj : pkg->objects_) {
          if (!util::all(obj_filters, *this, *obj))
            continue;
          printf("    contains %s / %s\n",
                 obj->dirname_.c_str(), obj->basename_.c_str());
        }
      }
    }
  }
}

void DB::ShowObjects(const FilterList    &pkg_filters,
                     const ObjFilterList &obj_filters)
{
  if (config_.json_ & JSONBits::Query)
    return ShowObjects_json(pkg_filters, obj_filters);

  if (!objects_.size()) {
    if (!config_.quiet_)
      printf("Objects: none\n");
    return;
  }
  if (!config_.quiet_)
    printf("Objects:\n");
  for (auto &obj : objects_) {
    if (!util::all(obj_filters, *this, *obj))
      continue;
    if (pkg_filters.size() &&
        (!obj->owner_ || !util::all(pkg_filters, *this, *obj->owner_)))
      continue;
    if (config_.quiet_)
      printf("%s/%s\n", obj->dirname_.c_str(), obj->basename_.c_str());
    else
      printf("  -> %s / %s\n", obj->dirname_.c_str(), obj->basename_.c_str());
    if (config_.verbosity_ < 1)
      continue;
    printf("     class: %u (%s)\n"
           "     data:  %u (%s)\n"
           "     osabi: %u (%s)\n",
           (unsigned)obj->ei_class_, obj->classString(),
           (unsigned)obj->ei_data_,  obj->dataString(),
           (unsigned)obj->ei_osabi_, obj->osabiString());
    if (obj->rpath_set_)
      printf("     rpath: %s\n", obj->rpath_.c_str());
    if (obj->runpath_set_)
      printf("     runpath: %s\n", obj->runpath_.c_str());
    if (obj->interpreter_.length())
      printf("     interpreter: %s\n", obj->interpreter_.c_str());
    if (config_.verbosity_ < 2)
      continue;
    printf("     finds:\n"); {
      for (auto &found : obj->req_found_)
        printf("       -> %s / %s\n",
               found->dirname_.c_str(), found->basename_.c_str());
    }
    printf("     misses:\n"); {
      for (auto &miss : obj->req_missing_)
        printf("       -> %s\n", miss.c_str());
    }
  }
}

void DB::ShowMissing() {
  if (config_.json_ & JSONBits::Query)
    return ShowMissing_json();

  if (!config_.quiet_)
    printf("Missing:\n");
  for (Elf *obj : objects_) {
    if (obj->req_missing_.empty())
      continue;
    if (config_.quiet_)
      printf("%s/%s\n", obj->dirname_.c_str(), obj->basename_.c_str());
    else
      printf("  -> %s / %s\n", obj->dirname_.c_str(), obj->basename_.c_str());
    for (auto &s : obj->req_missing_) {
      printf("    misses: %s\n", s.c_str());
    }
  }
}

void DB::ShowFound() {
  if (config_.json_ & JSONBits::Query)
    return ShowFound_json();

  if (!config_.quiet_)
    printf("Found:\n");
  for (Elf *obj : objects_) {
    if (obj->req_found_.empty())
      continue;
    if (config_.quiet_)
      printf("%s/%s\n", obj->dirname_.c_str(), obj->basename_.c_str());
    else
      printf("  -> %s / %s\n", obj->dirname_.c_str(), obj->basename_.c_str());
    for (auto &s : obj->req_found_)
      printf("    finds: %s\n", s->basename_.c_str());
  }
}

void DB::ShowFilelist(const FilterList    &pkg_filters,
                      const StrFilterList &str_filters)
{
  if (config_.json_ & JSONBits::Query)
    return ShowFilelist_json(pkg_filters, str_filters);

  for (auto &pkg : packages_) {
    if (!util::all(pkg_filters, *this, *pkg))
      continue;
    for (auto &file : pkg->filelist_) {
      if (!util::all(str_filters, file))
        continue;
      if (!config_.quiet_)
        printf("%s ", pkg->name_.c_str());
      printf("%s\n", file.c_str());
    }
  }
}

#ifdef PKGDEPDB_ENABLE_ALPM
void split_constraint(const string &full, string &op, string &ver) {
  op.clear();
  ver.clear();
  if (!full.length())
    return;
  op.append(1, full[0]);
  if (full[1] == '=') {
    op.append(1, full[1]);
    ver = full.substr(2);
  }
  else
    ver = full.substr(1);
}

static bool version_op(const string &op, const char *v1, const char *v2) {
  int res = alpm_pkg_vercmp(v1, v2);
  if ( (op == "="  && res == 0) ||
       (op == "!=" && res != 0) ||
       (op == ">"  && res >  0) ||
       (op == ">=" && res >= 0) ||
       (op == "<"  && res <  0) ||
       (op == "<=" && res <= 0) )
  {
    return true;
  }
  return false;
}

static bool version_satisfies(const string &dop,
                              const string &dver,
                              const string &pop,
                              const string &pver)
{
  // does the provided version pver satisfy the required version hver?
  int ret = alpm_pkg_vercmp(dver.c_str(), pver.c_str());
  if (dop == pop) {
    // want exact version, provided exact version
    if (dop == "=")  return ret == 0;
    // don't want some exact version (very odd case)
    if (dop == "!=") return ret != 0;
    // depending on >= A, so the provided must be >= A
    if (dop == ">=") return ret <  0;
    // and so on
    if (dop == ">")  return ret <= 0;
    if (dop == "<=") return ret >  0;
    if (dop == "<")  return ret >= 0;
    return false;
  }
  // depending on a specific version
  if (dop == "=")
    return false;
  // depending on something not being a specific version:
  if (dop == "!=") {
    if (pop == "=")  return ret != 0;
    if (pop == ">")  return ret >  0;
    if (pop == ">=") return ret >= 0;
    if (pop == "<")  return ret <  0;
    if (pop == "<=") return ret <= 0;
    return false;
  }
  // rest
  if (dop == ">=") {
    if (pop == "=")  return ret < 0;
    if (pop == ">")  return ret < 0;
    if (pop == ">=") return ret < 0;
    return false;
  }
  if (dop == ">") {
    if (pop == "=")  return ret <= 0;
    if (pop == ">")  return ret <= 0;
    if (pop == ">=") return ret <= 0;
    return false;
  }
  if (dop == "<=") {
    if (pop == "=")  return ret >  0;
    if (pop == "<")  return ret >  0;
    if (pop == "<=") return ret >  0;
    return false;
  }
  if (dop == "<") {
    if (pop == "=")  return ret >= 0;
    if (pop == "<")  return ret >= 0;
    if (pop == "<=") return ret >= 0;
    return false;
  }
  return false;
}

bool package_satisfies(const Package *other,
                       const string  &dep,
                       const string  &op,
                       const string  &ver)
{
  if (version_op(op, other->version_.c_str(), ver.c_str()))
    return true;
  for (auto &p : other->provides_) {
    if (std::get<0>(p) != dep)
      continue;
    string pop, pver;
    split_constraint(std::get<1>(p), pop, pver);
    if (version_satisfies(op, ver, pop, pver))
      return true;
  }
  return false;
}
#endif

static const Package* find_depend(const string     &dependency,
                                  const string     &constraint,
                                  const PkgMap     &pkgmap,
                                  const PkgListMap &providemap,
                                  const PkgListMap &replacemap)
{
  if (!dependency.length())
    return 0;

#ifdef PKGDEPDB_ENABLE_ALPM
  string op, ver;
  split_constraint(constraint, op, ver);
#endif

  auto find = pkgmap.find(dependency);
  if (find != pkgmap.end()) {
#ifdef PKGDEPDB_ENABLE_ALPM
    const Package *other = find->second;
    if (!ver.length() || package_satisfies(other, dependency, op, ver))
#endif
      return find->second;
  }
  // check for a providing package
  auto rep = replacemap.find(dependency);
  if (rep != replacemap.end()) {
#ifdef PKGDEPDB_ENABLE_ALPM
    if (!ver.length())
      return rep->second[0];
    for (auto other : rep->second) {
      if (package_satisfies(other, dependency, op, ver))
        return other;
    }
#else
    return rep->second[0];
#endif
  }

  rep = providemap.find(dependency);
  if (rep != providemap.end()) {
#ifdef PKGDEPDB_ENABLE_ALPM
    if (!ver.length())
      return rep->second[0];

    for (auto other : rep->second) {
      if (package_satisfies(other, dependency, op, ver))
        return other;
    }
#else
    return rep->second[0];
#endif
  }
  return nullptr;
}

static void install_recursive(vec<const Package*>         &packages,
                              PkgMap                      &installmap,
                              const Package               *pkg,
                              const PkgMap                &pkgmap,
                              const PkgListMap            &providemap,
                              const PkgListMap            &replacemap,
                              const bool                   showmsg,
                              const bool                   quiet)
{
  if (installmap.find(pkg->name_) != installmap.end())
    return;
  installmap[pkg->name_] = pkg;

  for (auto prov : pkg->provides_)
    installmap[std::get<0>(prov)] = pkg;
  for (auto repl : pkg->replaces_)
    installmap[std::get<0>(repl)] = pkg;
#ifdef PKGDEPDB_ENABLE_ALPM
  for (auto &full : pkg->conflicts_) {
    const string& conf = std::get<0>(full);
    string op, ver;
    split_constraint(std::get<1>(full), op, ver);

    auto found = installmap.find(conf);
    const Package *other = found->second;
    if (found == installmap.end() || other == pkg)
      continue;
    // found a conflict
    if (op.length() && ver.length()) {
      // version related conflict
      // pkg conflicts with {other} <op> {ver}
      if (!version_op(op, other->version_.c_str(), ver.c_str()))
        continue;
    }
    if (showmsg) {
      printf("%s%s conflicts with %s (%s-%s): { %s%s }\n",
             (quiet ? "" : "\r"),
             pkg->name_.c_str(),
             conf.c_str(),
             other->name_.c_str(),
             other->version_.c_str(),
             conf.c_str(), std::get<1>(full).c_str());
    }
  }
#endif
  packages.push_back(pkg);
  for (auto &dep : pkg->depends_) {
    auto found = find_depend(std::get<0>(dep), std::get<1>(dep),
                             pkgmap, providemap, replacemap);
    if (!found) {
      if (showmsg) {
        printf("%smissing package: %s depends on %s%s\n",
               (quiet ? "" : "\r"),
               pkg->name_.c_str(),
               std::get<0>(dep).c_str(), std::get<1>(dep).c_str());
      }
      continue;
    }
    install_recursive(packages, installmap, found,
                      pkgmap, providemap, replacemap, false, quiet);
  }
  for (auto &dep : pkg->optdepends_) {
    auto found = find_depend(std::get<0>(dep), std::get<1>(dep),
                             pkgmap, providemap, replacemap);
    if (!found) {
      if (showmsg) {
        printf("%smissing package: %s depends optionally on %s%s\n",
               (quiet ? "" : "\r"),
               pkg->name_.c_str(),
               std::get<0>(dep).c_str(), std::get<1>(dep).c_str());
      }
      continue;
    }
    install_recursive(packages, installmap, found,
                      pkgmap, providemap, replacemap, false, quiet);
  }
}

void DB::CheckIntegrity(const Package             *pkg,
                        const PkgMap              &pkgmap,
                        const PkgListMap          &providemap,
                        const PkgListMap          &replacemap,
                        const PkgMap              &basemap,
                        const ObjListMap          &objmap,
                        const vec<const Package*> &package_base,
                        const ObjFilterList       &obj_filters) const
{
  vec<const Package*> pulled(package_base);
  PkgMap              installmap(basemap);

  install_recursive(pulled, installmap, pkg,
                    pkgmap, providemap, replacemap, true, config_.quiet_);
  (void)objmap;

  StringSet needed;
  for (auto &obj : pkg->objects_) {
    if (!util::all(obj_filters, *this, *obj))
      continue;
    for (auto &need : obj->needed_) {
      auto fnd = objmap.find(need);
      if (fnd == objmap.end()) {
        needed.insert(need);
        continue;
      }
      bool found = false;
      for (auto &o : fnd->second) {
        if (std::find(pulled.begin(), pulled.end(), o->owner_) != pulled.end())
        {
          found = true;
          break;
        }
      }
      if (!found) {
        if (config_.verbosity_ > 0)
          printf("%s%s: %s not pulled in for %s/%s\n",
                 (config_.quiet_ ? "" : "\r"),
                 pkg->name_.c_str(),
                 need.c_str(),
                 obj->dirname_.c_str(), obj->basename_.c_str());
        needed.insert(need);
      }
    }
  }
  for (auto &n : needed) {
    printf("%s%s: doesn't pull in %s\n",
           (config_.quiet_ ? "" : "\r"),
           pkg->name_.c_str(),
           n.c_str());
  }
}

void DB::CheckIntegrity(const FilterList    &pkg_filters,
                        const ObjFilterList &obj_filters) const
{
  config_.Log(Message, "Looking for stale object files...\n");
  for (auto &o : objects_) {
    if (!o->owner_) {
      config_.Log(Message, "  object `%s/%s' has no owning package!\n",
                  o->dirname_.c_str(), o->basename_.c_str());
    }
  }

  config_.Log(Message, "Preparing data to check package dependencies...\n");
  // we assume here that std::map has better search performance than O(n)
  PkgMap     pkgmap;
  PkgListMap providemap, replacemap;
  ObjListMap objmap;

  for (auto &p: packages_) {
    pkgmap[p->name_] = p;
    auto addit = [](const Package *pkg, const string& name,
                    PkgListMap &map)
    {
      auto fnd = map.find(name);
      if (fnd == map.end())
        map.emplace(name, move(vec<const Package*>({pkg})));
      else
        fnd->second.push_back(pkg);
    };
    for (auto prov : p->provides_)
      addit(p, std::get<0>(prov), providemap);
    for (auto repl : p->replaces_)
      addit(p, std::get<0>(repl), replacemap);
  }

  for (auto &o: objects_) {
    if (o->owner_)
      objmap[o->basename_].push_back(o);
  }

  // install base system:
  vec<const Package*> base;
  PkgMap              basemap;

  for (auto &basepkg : base_packages_) {
    auto p = pkgmap.find(basepkg);
    if (p != pkgmap.end()) {
      base.push_back(p->second);
      basemap[basepkg] = p->second;
    }
  }

  // print some stats
  config_.Log(Message,
      "packages: %lu, provides: %lu, replacements: %lu, objects: %lu\n",
      (unsigned long)pkgmap.size(),
      (unsigned long)providemap.size(),
      (unsigned long)replacemap.size(),
      (unsigned long)objmap.size());

  bool cfgquiet = config_.quiet_;
  double fac = 100.0 / double(packages_.size());
  unsigned int pc = 100;
  auto status = [fac,&pc,cfgquiet](unsigned long at,
                                   unsigned long cnt,
                                   unsigned long threads)
  {
    auto newpc = (unsigned int)(fac * double(at));
    if (newpc == pc)
      return;
    pc = newpc;
    if (!cfgquiet)
      printf("\rpackages: %3u%% (%lu / %lu) [%lu]",
             pc, at, cnt, threads);
    fflush(stdout);
    if (at == cnt)
      printf("\n");
  };

  config_.Log(Message, "Checking package dependencies...\n");
#ifdef PKGDEPDB_ENABLE_THREADS
  if (config_.max_jobs_ == 1) {
#endif
    status(0, packages_.size(), 1);
    for (size_t i = 0; i != packages_.size(); ++i) {
      if (!util::all(pkg_filters, *this, *packages_[i]))
        continue;
      CheckIntegrity(packages_[i], pkgmap, providemap, replacemap,
                      basemap, objmap, base, obj_filters);
      if (!config_.quiet_)
        status(i, packages_.size(), 1);
    }
#ifdef PKGDEPDB_ENABLE_THREADS
  } else {
    auto merger = [](vec<int> &&n) {
      (void)n;
    };
    auto worker =
      [this,&pkgmap,&providemap,&replacemap,
       &objmap,&base,&basemap,&obj_filters,&pkg_filters]
    (std::atomic_ulong *count, size_t from, size_t to, int &dummy) {
      (void)dummy;

      for (size_t i = from; i != to; ++i) {
        const Package *pkg = packages_[i];
        if (util::all(pkg_filters, *this, *pkg)) {
          CheckIntegrity(pkg, pkgmap, providemap, replacemap,
                          basemap, objmap, base, obj_filters);
        }
        if (count)
          ++*count;
      }
    };
    thread::work<int>(packages_.size(), status, worker, merger, config_);
  }
#endif

  config_.Log(Message, "Checking for file conflicts...\n");
  std::map<strref,vec<const Package*>> file_counter;
  for (auto &pkg : packages_) {
    for (auto &file : pkg->filelist_) {
      file_counter[file].push_back(pkg);
    }
  }
  for (auto &file : file_counter) {
    auto &pkgs = std::get<1>(file);
    if (pkgs.size() < 2)
      continue;

    vec<const Package*> realpkgs;
    // Do not consider two conflicting packages which contain
    // the same files to be file-conflicting.
    for (auto &a : pkgs) {
      bool conflict = false;
      for (auto &b : pkgs) {
        if (&a == &b) continue;
        if ( (conflict = a->ConflictsWith(*b)) )
          break;
        if ( (conflict = a->Replaces(*b)) )
          break;
      }
      if (!conflict)
        realpkgs.push_back(a);
    }

    if (realpkgs.size() > 1) {
      printf("%zu packages contain file: %s\n",
             realpkgs.size(), std::get<0>(file)->c_str());
      if (config_.verbosity_) {
        for (auto &p : realpkgs)
          printf("\t%s\n", p->name_.c_str());
      }
    }
  }
}

} // ::pkgdepdb
