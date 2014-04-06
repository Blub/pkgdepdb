#include <memory>
#include <algorithm>
#include <utility>

#ifdef ENABLE_THREADS
#  include <atomic>
#  include <thread>
#  include <future>
#  include <unistd.h>
#endif

#ifdef WITH_ALPM
#  include <alpm.h>
#endif

#include "main.h"

using ObjClass = uint32_t;

static inline ObjClass
getObjClass(unsigned char ei_class, unsigned char ei_data, unsigned char ei_osabi) {
  return static_cast<ObjClass>((ei_data << 16) | (ei_class << 8) | ei_osabi);
}

static inline ObjClass
getObjClass(Elf *elf) {
  return getObjClass(elf->ei_class_, elf->ei_data_, elf->ei_osabi_);
}

DB::DB() {
  loaded_version_           = DB::CURRENT;
  contains_package_depends_ = false;
  contains_groups_          = false;
  contains_filelists_       = false;
  strict_linking_           = false;
}

DB::~DB() {
  for (auto &pkg : packages_)
    delete pkg;
}

template<typename T>
static void stdreplace(T &what, const T &with)
{
  what.~T();
  new (&what) T(with);
}

DB::DB(bool wiped, const DB &copy)
: name_                (copy.name_),
  library_path_        (copy.library_path_),
  ignore_file_rules_   (copy.ignore_file_rules_),
  package_library_path_(copy.package_library_path_),
  base_packages_       (copy.base_packages_)
{
  loaded_version_ = copy.loaded_version_;
  strict_linking_ = copy.strict_linking_;
  if (!wiped) {
    stdreplace(packages_, copy.packages_);
    stdreplace(objects_,  copy.objects_);
  }
}

PackageList::const_iterator
DB::find_pkg_i(const std::string& name) const
{
  return std::find_if(packages_.begin(), packages_.end(),
    [&name](const Package *pkg) { return pkg->name_ == name; });
}

Package*
DB::find_pkg(const std::string& name) const
{
  auto pkg = find_pkg_i(name);
  return (pkg != packages_.end()) ? *pkg : nullptr;
}

bool
DB::wipe_packages()
{
  if (empty())
    return false;
  objects_.clear();
  packages_.clear();
  return true;
}

bool
DB::wipe_filelists()
{
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

const StringList*
DB::get_obj_libpath(const Elf *elf) const {
  return elf->owner_ ? get_pkg_libpath(elf->owner_) : nullptr;
}

const StringList*
DB::get_pkg_libpath(const Package *pkg) const {
  if (!package_library_path_.size())
    return nullptr;

  auto iter = package_library_path_.find(pkg->name_);
  if (iter != package_library_path_.end())
    return &iter->second;
  return nullptr;
}

bool
DB::delete_package(const std::string& name)
{
  const Package *old; {
    auto pkgiter = find_pkg_i(name);
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
    // for each object which depends on this object, search for a replacing object
      auto ref = std::find(seeker->req_found_.begin(),
                           seeker->req_found_.end(),
                           elf);
      if (ref == seeker->req_found_.end())
        continue;
      seeker->req_found_.erase(ref);

      const StringList *libpaths = get_obj_libpath(seeker);
      if (Elf *other = find_for(seeker, elf->basename_, libpaths))
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

static bool
pathlist_contains(const std::string& list, const std::string& path)
{
  size_t at = 0;
  size_t to = list.find_first_of(':', 0);
  while (to != std::string::npos) {
    if (list.compare(at, to-at, path) == 0)
      return true;
    at = to+1;
    to = list.find_first_of(':', at);
  }
  if (list.compare(at, std::string::npos, path) == 0)
    return true;
  return false;
}

bool
DB::elf_finds(const Elf *elf, const std::string& path, const StringList *extrapaths) const
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

  if (std::find(library_path_.begin(), library_path_.end(), path) != library_path_.end())
    return true;

  if (extrapaths) {
    if (std::find(extrapaths->begin(), extrapaths->end(), path) != extrapaths->end())
      return true;
  }

  return false;
}

bool
DB::install_package(Package* &&pkg)
{
  if (!delete_package(pkg->name_))
    return false;

  packages_.push_back(pkg);
  if (pkg->depends_.size()    ||
      pkg->optdepends_.size() ||
      pkg->replaces_.size()   ||
      pkg->conflicts_.size()  ||
      pkg->provides_.size())
  {
    contains_package_depends_ = true;
  }
  if (pkg->groups_.size())
    contains_groups_ = true;
  if (pkg->filelist_.size())
    contains_filelists_ = true;

  const StringList *libpaths = get_pkg_libpath(pkg);

  for (auto &obj : pkg->objects_)
    objects_.push_back(obj);
  // loop anew since we need to also be able to found our own packages
  for (auto &obj : pkg->objects_)
    link_object_do(obj, pkg);

  // check for packages which are looking for any of our packages
  for (auto &seeker : objects_) {
    for (auto &obj : pkg->objects_) {
      if (!seeker->can_use(*obj, strict_linking_) ||
          !elf_finds(seeker, obj->dirname_, libpaths))
      {
        continue;
      }

      if (0 != seeker->req_missing_.erase(obj->basename_))
        seeker->req_found_.insert(obj);
    }
  }
  return true;
}

Elf*
DB::find_for(const Elf *obj, const std::string& needed, const StringList *extrapath) const
{
  log(Debug, "dependency of %s/%s   :  %s\n", obj->dirname_.c_str(), obj->basename_.c_str(), needed.c_str());
  for (auto &lib : objects_) {
    if (!obj->can_use(*lib, strict_linking_)) {
      log(Debug, "  skipping %s/%s (objclass)\n", lib->dirname_.c_str(), lib->basename_.c_str());
      continue;
    }
    if (lib->basename_    != needed) {
      log(Debug, "  skipping %s/%s (wrong name)\n", lib->dirname_.c_str(), lib->basename_.c_str());
      continue;
    }
    if (!elf_finds(obj, lib->dirname_, extrapath)) {
      log(Debug, "  skipping %s/%s (not visible)\n", lib->dirname_.c_str(), lib->basename_.c_str());
      continue;
    }
    // same class, same name, and visible...
    return lib;
  }
  return 0;
}

void
DB::link_object_do(Elf *obj, const Package *owner)
{
  obj->req_found_.clear();
  obj->req_missing_.clear();
  link_object(obj, owner, obj->req_found_, obj->req_missing_);
}

void
DB::link_object(Elf *obj, const Package *owner, ObjectSet &req_found, StringSet &req_missing) const
{
  if (ignore_file_rules_.size()) {
    std::string full = obj->dirname_ + "/" + obj->basename_;
    if (ignore_file_rules_.find(full) != ignore_file_rules_.end())
      return;
  }

  const StringList *libpaths = get_pkg_libpath(owner);

  for (auto &needed : obj->needed_) {
    Elf *found = find_for(obj, needed, libpaths);
    if (found)
      req_found.insert(found);
    else if (assume_found_rules_.find(needed) == assume_found_rules_.end())
      req_missing.insert(needed);
  }
}

#ifdef ENABLE_THREADS
namespace thread {
  static unsigned int ncpus_init() {
    long v = sysconf(_SC_NPROCESSORS_CONF);
    return (v <= 0 ? 1 : (unsigned int)v);
  }

  static unsigned int ncpus = ncpus_init();

  template<typename PerThread>
  void
  work(unsigned long  Count,
       std::function<void(unsigned long at, unsigned long cnt, unsigned long threads)>
                      StatusPrinter,
       std::function<void(std::atomic_ulong*,
                          size_t from,
                          size_t to,
                          PerThread&)>
                      Worker,
       std::function<void(std::vector<PerThread>&&)>
                      Merger)
  {
    unsigned long threadcount = thread::ncpus;
    if (opt_max_jobs >= 1 && opt_max_jobs < threadcount)
      threadcount = opt_max_jobs;

    unsigned long  obj_per_thread = Count / threadcount;
    if (!opt_quiet)
      StatusPrinter(0, Count, threadcount);

    if (threadcount == 1) {
      for (unsigned long i = 0; i != Count; ++i) {
        PerThread Data;
        Worker(nullptr, i, i+1, Data);
        if (!opt_quiet)
          StatusPrinter(i, Count, 1);
      }
      return;
    }

    // data created by threads, to be merged in the merger
    std::vector<PerThread> Data;
    Data.resize(threadcount);

    std::atomic_ulong         counter(0);
    std::vector<std::thread*> threads;

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
    if (!opt_quiet) {
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
    Merger(std::move(Data));
    if (!opt_quiet)
      StatusPrinter(Count, Count, threadcount);
  }
}

void
DB::relink_all_threaded()
{
  //using FoundMap   = std::map<Elf*, ObjectSet>;
  //using MissingMap = std::map<Elf*, StringSet>;
  //using Tuple      = std::tuple<FoundMap, MissingMap>;
  auto worker = [this](std::atomic_ulong *count, size_t from, size_t to, int&) {
    //FoundMap   *f = &std::get<0>(tup);
    //MissingMap *m = &std::get<1>(tup);
    for (size_t i = from; i != to; ++i) {
      const Package *pkg = this->packages_[i];

      for (auto &obj : pkg->objects_) {
        this->link_object_do(obj, pkg);
        //ObjectSet req_found;
        //StringSet req_missing;
        //this->link_object(obj, pkg, req_found, req_missing);
        //(*f)[obj] = std::move(req_found);
        //(*m)[obj] = std::move(req_missing);
      }

      if (count && !opt_quiet)
        (*count)++;
    }
  };
  auto merger = [this](std::vector<int> &&) {
    //for (auto &t : tup) {
    //  FoundMap   &found = std::get<0>(t);
    //  MissingMap &missing = std::get<1>(t);
    //  for (auto &f : found)
    //    f.first->req_found = std::move(f.second);
    //  for (auto &m : missing)
    //    m.first->req_missing = std::move(m.second);
    //}
  };
  double fac = 100.0 / double(packages_.size());
  unsigned int pc = 1000;
  auto status = [fac, &pc](unsigned long at, unsigned long cnt, unsigned long threadcount) {
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
  thread::work<int>(packages_.size(), status, worker, merger);
}
#endif

void
DB::relink_all()
{
  if (!packages_.size())
    return;

#ifdef ENABLE_THREADS
  if (opt_max_jobs     != 1   &&
      thread::ncpus    >  1   &&
      packages_.size() >  100 &&
      objects_.size()  >= 300)
  {
    return relink_all_threaded();
  }
#endif

  unsigned long pkgcount = packages_.size();
  double        fac   = 100.0 / double(pkgcount);
  unsigned long count = 0;
  unsigned int  pc    = 0;
  if (!opt_quiet) {
    printf("relinking: 0%% (0 / %lu packages)", pkgcount);
    fflush(stdout);
  }
  for (auto &pkg : packages_) {
    for (auto &obj : pkg->objects_) {
      link_object_do(obj, pkg);
    }
    if (!opt_quiet) {
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
  if (!opt_quiet) {
    printf("\rrelinking: 100%% (%lu / %lu packages)\n",
           count, pkgcount);
  }
}

void
DB::fix_paths()
{
  for (auto &obj : objects_) {
    fixpathlist(obj->rpath_);
    fixpathlist(obj->runpath_);
  }
}

bool
DB::empty() const
{
  return packages_.size() == 0 &&
         objects_.size()  == 0;
}

bool
DB::ld_clear()
{
  if (library_path_.size()) {
    library_path_.clear();
    return true;
  }
  return false;
}

static std::string
fixcpath(const std::string& dir)
{
  std::string s(dir);
  fixpath(s);
  return std::move(dir);
}

bool
DB::ld_append(const std::string& dir)
{
  return ld_insert(fixcpath(dir), library_path_.size());
}

bool
DB::ld_prepend(const std::string& dir)
{
  return ld_insert(fixcpath(dir), 0);
}

bool
DB::ld_delete(size_t i)
{
  if (!library_path_.size() || i >= library_path_.size())
    return false;
  library_path_.erase(library_path_.begin() + i);
  return true;
}

bool
DB::ld_delete(const std::string& dir_)
{
  if (!dir_.length())
    return false;
  if (dir_[0] >= '0' && dir_[0] <= '9') {
    return ld_delete(strtoul(dir_.c_str(), nullptr, 0));
  }
  std::string dir(dir_);
  fixpath(dir);
  auto old = std::find(library_path_.begin(), library_path_.end(), dir);
  if (old != library_path_.end()) {
    library_path_.erase(old);
    return true;
  }
  return false;
}

bool
DB::ld_insert(const std::string& dir_, size_t i)
{
  std::string dir(dir_);
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

bool
DB::pkg_ld_insert(const std::string& package, const std::string& dir_, size_t i)
{
  std::string dir(dir_);
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

bool
DB::pkg_ld_delete(const std::string& package, const std::string& dir_)
{
  std::string dir(dir_);
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

bool
DB::pkg_ld_delete(const std::string& package, size_t i)
{
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

bool
DB::pkg_ld_clear(const std::string& package)
{
  auto iter = package_library_path_.find(package);
  if (iter == package_library_path_.end())
    return false;

  package_library_path_.erase(iter);
  return true;
}

bool
DB::ignore_file(const std::string& filename)
{
  return std::get<1>(ignore_file_rules_.insert(fixcpath(filename)));
}

bool
DB::unignore_file(const std::string& filename)
{
  return (ignore_file_rules_.erase(fixcpath(filename)) > 0);
}

bool
DB::unignore_file(size_t id)
{
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

bool
DB::assume_found(const std::string& name)
{
  return std::get<1>(assume_found_rules_.insert(name));
}

bool
DB::unassume_found(const std::string& name)
{
  return (assume_found_rules_.erase(name) > 0);
}

bool
DB::unassume_found(size_t id)
{
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

bool
DB::add_base_package(const std::string& name)
{
  return std::get<1>(base_packages_.insert(name));
}

bool
DB::remove_base_package(const std::string& name)
{
  return (base_packages_.erase(name) > 0);
}

bool
DB::remove_base_package(size_t id)
{
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

void
DB::show_info()
{
  if (opt_json & JSONBits::Query)
    return show_info_json();

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

bool
DB::is_broken(const Elf *obj) const
{
  return obj->req_missing_.size() != 0;
}

bool
DB::is_empty(const Package *pkg, const ObjFilterList &filters) const
{
  size_t vis = 0;
  for (auto &obj : pkg->objects_) {
    if (util::all(filters, *this, *obj))
      ++vis;
  }
  return vis == 0;
}

bool
DB::is_broken(const Package *pkg) const
{
  for (auto &obj : pkg->objects_) {
    if (is_broken(obj))
      return true;
  }
  return false;
}

void
DB::show_packages(bool filter_broken,
                  bool filter_notempty,
                  const FilterList &pkg_filters,
                  const ObjFilterList &obj_filters)
{
  if (opt_json & JSONBits::Query)
    return show_packages_json(filter_broken, filter_notempty, pkg_filters, obj_filters);

  if (!opt_quiet)
    printf("Packages:%s\n", (filter_broken ? " (filter: 'broken')" : ""));
  for (auto &pkg : packages_) {
    if (!util::all(pkg_filters, *this, *pkg))
      continue;
    if (filter_broken && !is_broken(pkg))
      continue;
    if (filter_notempty && is_empty(pkg, obj_filters))
      continue;
    if (opt_quiet)
      printf("%s\n", pkg->name_.c_str());
    else
      printf("  -> %s - %s\n", pkg->name_.c_str(), pkg->version_.c_str());
    if (opt_verbosity >= 1) {
      for (auto &grp : pkg->groups_)
        printf("    is in group: %s\n", grp.c_str());
      for (auto &dep : pkg->depends_)
        printf("    depends on: %s\n", dep.c_str());
      for (auto &dep : pkg->optdepends_)
        printf("    depends optionally on: %s\n", dep.c_str());
      for (auto &ent : pkg->provides_)
        printf("    provides: %s\n", ent.c_str());
      for (auto &ent : pkg->replaces_)
        printf("    replaces: %s\n", ent.c_str());
      for (auto &ent : pkg->conflicts_)
        printf("    conflicts with: %s\n", ent.c_str());
      if (filter_broken) {
        for (auto &obj : pkg->objects_) {
          if (!util::all(obj_filters, *this, *obj))
            continue;
          if (is_broken(obj)) {
            printf("    broken: %s / %s\n", obj->dirname_.c_str(), obj->basename_.c_str());
            if (opt_verbosity >= 2) {
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
          printf("    contains %s / %s\n", obj->dirname_.c_str(), obj->basename_.c_str());
        }
      }
    }
  }
}

void
DB::show_objects(const FilterList &pkg_filters, const ObjFilterList &obj_filters)
{
  if (opt_json & JSONBits::Query)
    return show_objects_json(pkg_filters, obj_filters);

  if (!objects_.size()) {
    if (!opt_quiet)
      printf("Objects: none\n");
    return;
  }
  if (!opt_quiet)
    printf("Objects:\n");
  for (auto &obj : objects_) {
    if (!util::all(obj_filters, *this, *obj))
      continue;
    if (pkg_filters.size() && (!obj->owner_ || !util::all(pkg_filters, *this, *obj->owner_)))
      continue;
    if (opt_quiet)
      printf("%s/%s\n", obj->dirname_.c_str(), obj->basename_.c_str());
    else
      printf("  -> %s / %s\n", obj->dirname_.c_str(), obj->basename_.c_str());
    if (opt_verbosity < 1)
      continue;
    printf("     class: %u (%s)\n", (unsigned)obj->ei_class_, obj->classString());
    printf("     data:  %u (%s)\n", (unsigned)obj->ei_data_,  obj->dataString());
    printf("     osabi: %u (%s)\n", (unsigned)obj->ei_osabi_, obj->osabiString());
    if (obj->rpath_set_)
      printf("     rpath: %s\n", obj->rpath_.c_str());
    if (obj->runpath_set_)
      printf("     runpath: %s\n", obj->runpath_.c_str());
    if (opt_verbosity < 2)
      continue;
    printf("     finds:\n"); {
      for (auto &found : obj->req_found_)
        printf("       -> %s / %s\n", found->dirname_.c_str(), found->basename_.c_str());
    }
    printf("     misses:\n"); {
      for (auto &miss : obj->req_missing_)
        printf("       -> %s\n", miss.c_str());
    }
  }
}

void
DB::show_missing()
{
  if (opt_json & JSONBits::Query)
    return show_missing_json();

  if (!opt_quiet)
    printf("Missing:\n");
  for (Elf *obj : objects_) {
    if (obj->req_missing_.empty())
      continue;
    if (opt_quiet)
      printf("%s/%s\n", obj->dirname_.c_str(), obj->basename_.c_str());
    else
      printf("  -> %s / %s\n", obj->dirname_.c_str(), obj->basename_.c_str());
    for (auto &s : obj->req_missing_) {
      printf("    misses: %s\n", s.c_str());
    }
  }
}

void
DB::show_found()
{
  if (opt_json & JSONBits::Query)
    return show_found_json();

  if (!opt_quiet)
    printf("Found:\n");
  for (Elf *obj : objects_) {
    if (obj->req_found_.empty())
      continue;
    if (opt_quiet)
      printf("%s/%s\n", obj->dirname_.c_str(), obj->basename_.c_str());
    else
      printf("  -> %s / %s\n", obj->dirname_.c_str(), obj->basename_.c_str());
    for (auto &s : obj->req_found_)
      printf("    finds: %s\n", s->basename_.c_str());
  }
}

void
DB::show_filelist(const FilterList &pkg_filters,
                  const StrFilterList &str_filters)
{
  if (opt_json & JSONBits::Query)
    return show_filelist_json(pkg_filters, str_filters);

  for (auto &pkg : packages_) {
    if (!util::all(pkg_filters, *this, *pkg))
      continue;
    for (auto &file : pkg->filelist_) {
      if (!util::all(str_filters, file))
        continue;
      if (!opt_quiet)
        printf("%s ", pkg->name_.c_str());
      printf("%s\n", file.c_str());
    }
  }
}

static void
strip_version(std::string &s)
{
  size_t from = s.find_first_of("=<>!");
  if (from != std::string::npos)
    s.erase(from);
}

#ifdef WITH_ALPM
bool
split_depstring(const std::string &full, std::string &name, std::string &op, std::string &ver)
{
  size_t opidx = full.find_first_of("=<>!");
  if (opidx != std::string::npos) {
    name = full.substr(0, opidx);

    op.append(1, full[opidx++]);
    if (opidx >= full.length())
      return false;

    if (full[opidx] == '=') {
      op.append(1, '=');
      ++opidx;
    }
    if (opidx >= full.length())
      return false;
    ver = full.substr(opidx);
  }
  else
    name = full;
  return true;
}

static bool
version_op(const std::string &op, const char *v1, const char *v2)
{
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

static bool
version_satisfies(const std::string &dop, const std::string &dver, const std::string &pop, const std::string &pver)
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

bool
package_satisfies(const Package *other, const std::string &dep, const std::string &op, const std::string &ver)
{
  if (version_op(op, other->version_.c_str(), ver.c_str()))
    return true;
  for (auto &p : other->provides_) {
    std::string prov, pop, pver;
    split_depstring(p, prov, pop, pver);
    if (prov != dep)
      continue;
    if (version_satisfies(op, ver, pop, pver))
      return true;
  }
  return false;
}
#endif

static const Package*
find_depend(const std::string &dep_, const PkgMap &pkgmap, const PkgListMap &providemap, const PkgListMap &replacemap)
{
  if (!dep_.length())
    return 0;

#ifdef WITH_ALPM
  std::string dep, op, ver;
  split_depstring(dep_, dep, op, ver);
#else
  std::string dep(dep_);
  strip_version(dep);
#endif

  auto find = pkgmap.find(dep);
  if (find != pkgmap.end()) {
#ifdef WITH_ALPM
    const Package *other = find->second;
    if (!ver.length() || package_satisfies(other, dep, op, ver))
#endif
      return find->second;
  }
  // check for a providing package
  auto rep = replacemap.find(dep);
  if (rep != replacemap.end()) {
#ifdef WITH_ALPM
    if (!ver.length())
      return rep->second[0];
    for (auto other : rep->second) {
      if (package_satisfies(other, dep, op, ver))
        return other;
    }
#else
    return rep->second[0];
#endif
  }

  rep = providemap.find(dep);
  if (rep != providemap.end()) {
#ifdef WITH_ALPM
    if (!ver.length())
      return rep->second[0];

    for (auto other : rep->second) {
      if (package_satisfies(other, dep, op, ver))
        return other;
    }
#else
    return rep->second[0];
#endif
  }
  return nullptr;
}

static void
install_recursive(std::vector<const Package*> &packages,
                  PkgMap                      &installmap,
                  const Package               *pkg,
                  const PkgMap                &pkgmap,
                  const PkgListMap            &providemap,
                  const PkgListMap            &replacemap,
                  const bool                   showmsg)
{
  if (installmap.find(pkg->name_) != installmap.end())
    return;
  installmap[pkg->name_] = pkg;

  for (auto prov : pkg->provides_) {
    strip_version(prov);
    installmap[prov] = pkg;
  }
  for (auto repl : pkg->replaces_) {
    strip_version(repl);
    installmap[repl] = pkg;
  }
#ifdef WITH_ALPM
  for (auto &full : pkg->conflicts_) {
    std::string conf, op, ver;
    if (!split_depstring(full, conf, op, ver))
      break;

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
      printf("%s%s conflicts with %s (%s-%s): { %s }\n",
             (opt_quiet ? "" : "\r"),
             pkg->name_.c_str(),
             conf.c_str(),
             other->name_.c_str(),
             other->version_.c_str(),
             full.c_str());
    }
  }
#endif
  packages.push_back(pkg);
  for (auto &dep : pkg->depends_) {
    auto found = find_depend(dep, pkgmap, providemap, replacemap);
    if (!found) {
      if (showmsg) {
        printf("%smissing package: %s depends on %s\n",
               (opt_quiet ? "" : "\r"),
               pkg->name_.c_str(),
               dep.c_str());
      }
      continue;
    }
    install_recursive(packages, installmap, found, pkgmap, providemap, replacemap, false);
  }
  for (auto &dep : pkg->optdepends_) {
    auto found = find_depend(dep, pkgmap, providemap, replacemap);
    if (!found) {
      if (showmsg) {
        printf("%smissing package: %s depends optionally on %s\n",
               (opt_quiet ? "" : "\r"),
               pkg->name_.c_str(),
               dep.c_str());
      }
      continue;
    }
    install_recursive(packages, installmap, found, pkgmap, providemap, replacemap, false);
  }
}

void
DB::check_integrity(const Package    *pkg,
                    const PkgMap     &pkgmap,
                    const PkgListMap &providemap,
                    const PkgListMap &replacemap,
                    const PkgMap     &basemap,
                    const ObjListMap &objmap,
                    const std::vector<const Package*> &package_base,
                    const ObjFilterList &obj_filters) const
{
  std::vector<const Package*> pulled(package_base);
  PkgMap                      installmap(basemap);
  install_recursive(pulled, installmap, pkg, pkgmap, providemap, replacemap, true);
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
        if (opt_verbosity > 0)
          printf("%s%s: %s not pulled in for %s/%s\n",
                 (opt_quiet ? "" : "\r"),
                 pkg->name_.c_str(),
                 need.c_str(),
                 obj->dirname_.c_str(), obj->basename_.c_str());
        needed.insert(need);
      }
    }
  }
  for (auto &n : needed) {
    printf("%s%s: doesn't pull in %s\n",
           (opt_quiet ? "" : "\r"),
           pkg->name_.c_str(),
           n.c_str());
  }
}

void
DB::check_integrity(const FilterList    &pkg_filters,
                    const ObjFilterList &obj_filters) const
{
  log(Message, "Looking for stale object files...\n");
  for (auto &o : objects_) {
    if (!o->owner_) {
      log(Message, "  object `%s/%s' has no owning package!\n",
          o->dirname_.c_str(), o->basename_.c_str());
    }
  }

  log(Message, "Preparing data to check package dependencies...\n");
  // we assume here that std::map has better search performance than O(n)
  PkgMap     pkgmap;
  PkgListMap providemap, replacemap;
  ObjListMap objmap;

  for (auto &p: packages_) {
    pkgmap[p->name_] = p;
    auto addit = [](const Package *pkg, std::string /*copy*/ name, PkgListMap &map) {
      strip_version(name);
      auto fnd = map.find(name);
      if (fnd == map.end())
        map.emplace(name, std::move(std::vector<const Package*>({pkg})));
      else
        fnd->second.push_back(pkg);
    };
    for (auto prov : p->provides_)
      addit(p, prov, providemap);
    for (auto repl : p->replaces_)
      addit(p, repl, replacemap);
  }

  for (auto &o: objects_) {
    if (o->owner_)
      objmap[o->basename_].push_back(o);
  }

  // install base system:
  std::vector<const Package*> base;
  PkgMap                      basemap;

  for (auto &basepkg : base_packages_) {
    auto p = pkgmap.find(basepkg);
    if (p != pkgmap.end()) {
      base.push_back(p->second);
      basemap[basepkg] = p->second;
    }
  }

  // print some stats
  log(Message, "packages: %lu, provides: %lu, replacements: %lu, objects: %lu\n",
               (unsigned long)pkgmap.size(),
               (unsigned long)providemap.size(),
               (unsigned long)replacemap.size(),
               (unsigned long)objmap.size());

  double fac = 100.0 / double(packages_.size());
  unsigned int pc = 100;
  auto status = [fac,&pc](unsigned long at, unsigned long cnt, unsigned long threads) {
    auto newpc = (unsigned int)(fac * double(at));
    if (newpc == pc)
      return;
    pc = newpc;
    if (!opt_quiet)
      printf("\rpackages: %3u%% (%lu / %lu) [%lu]",
             pc, at, cnt, threads);
    fflush(stdout);
    if (at == cnt)
      printf("\n");
  };

  log(Message, "Checking package dependencies...\n");
#ifdef ENABLE_THREADS
  if (opt_max_jobs == 1) {
#endif
    status(0, packages_.size(), 1);
    for (size_t i = 0; i != packages_.size(); ++i) {
      if (!util::all(pkg_filters, *this, *packages_[i]))
        continue;
      check_integrity(packages_[i], pkgmap, providemap, replacemap,
                      basemap, objmap, base, obj_filters);
      if (!opt_quiet)
        status(i, packages_.size(), 1);
    }
#ifdef ENABLE_THREADS
  } else {
    auto merger = [](std::vector<int> &&n) {
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
          check_integrity(pkg, pkgmap, providemap, replacemap,
                          basemap, objmap, base, obj_filters);
        }
        if (count)
          ++*count;
      }
    };
    thread::work<int>(packages_.size(), status, worker, merger);
  }
#endif

  log(Message, "Checking for file conflicts...\n");
  std::map<strref,std::vector<const Package*>> file_counter;
  for (auto &pkg : packages_) {
    for (auto &file : pkg->filelist_) {
      file_counter[file].push_back(pkg);
    }
  }
  for (auto &file : file_counter) {
    auto &pkgs = std::get<1>(file);
    if (pkgs.size() < 2)
      continue;

    std::vector<const Package*> realpkgs;
    // Do not consider two conflicting packages which contain the same files to be
    // file-conflicting.
    for (auto &a : pkgs) {
      bool conflict = false;
      for (auto &b : pkgs) {
        if (&a == &b) continue;
        if ( (conflict = a->conflicts_with(*b)) )
          break;
      }
      if (!conflict)
        realpkgs.push_back(a);
    }

    if (realpkgs.size() > 1) {
      printf("%zu packages contain file: %s\n", realpkgs.size(), std::get<0>(file)->c_str());
      if (opt_verbosity) {
        for (auto &p : realpkgs)
          printf("\t%s\n", p->name_.c_str());
      }
    }
  }
}
