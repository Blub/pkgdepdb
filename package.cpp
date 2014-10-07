#include <memory>

#include <archive.h>
#include <archive_entry.h>

#include "main.h"
#include "elf.h"
#include "package.h"

namespace pkgdepdb {

static bool care_about(struct archive_entry *entry, mode_t mode) {
#if 0
  if (AE_IFLNK == (mode & AE_IFLNK)) {
    // ignoring symlinks for now
    return false;
  }
#endif

  if (AE_IFREG != (mode & AE_IFREG)) {
    // not a regular file
    return false;
  }

  if (!archive_entry_size_is_set(entry)) {
    // ignore files which have no size...
    return false;
  }

  return true;
}

// because isspace(3) is locale dependent
static bool c_isspace(const char c) {
  return (c == ' '  || c == '\t' ||
          c == '\n' || c == '\r' ||
          c == '\v' || c == '\f');
}

static bool ends_word(const char c) {
  return c_isspace(c) || c == '=';
}

static
void split_depstring(const string &full, string &name, string &constraint) {
  size_t opidx = full.find_first_of("=<>!");
  if (opidx == string::npos) {
    name = full;
    constraint.clear();
  } else {
    name = full.substr(0, opidx);
    constraint = full.substr(opidx);
  }
}

// NOTE:
// Judgding from pacman/libalpm source code this function
// is way less strict about the formatting, as we skip whitespace
// between every word, whereas pacman matches /^(\w+) = (.*)$/ exactly.
static bool read_info(Package *pkg, struct archive *tar, const size_t size,
                      const Config& optconfig)
{
  vec<char> data(size);
  ssize_t rc = archive_read_data(tar, &data[0], size);
  if ((size_t)rc != size) {
    optconfig.Log(Error, "failed to read .PKGINFO");
    return false;
  }

  string str(&data[0], data.size());

  return pkg->ReadInfo(str, size, optconfig);
}

bool Package::ReadInfo(const string& str, const size_t size,
                       const Config& optconfig)
{
  size_t pos = 0;
  auto skipwhite = [&]() {
    while (pos < size && c_isspace(str[pos]))
      ++pos;
  };

  auto skipline = [&]() {
    while (pos < size && str[pos] != '\n')
      ++pos;
  };

  auto getvalue = [&](const char *entryname, string &out) -> bool {
    skipwhite();
    if (pos >= size) {
      optconfig.Log(Error, "invalid %s entry in .PKGINFO", entryname);
      return false;
    }
    if (str[pos] != '=') {
      optconfig.Log(Error, "Error in .PKGINFO");
      return false;
    }
    ++pos;
    skipwhite();
    if (pos >= size) {
      optconfig.Log(Error, "invalid %s entry in .PKGINFO", entryname);
      return false;
    }
    size_t to = str.find_first_of(" \n\r\t", pos);
    out = str.substr(pos, to-pos);
    skipline();
    return true;
  };

  auto isentry = [&](const char *what, size_t len) -> bool {
    if (size-pos > len &&
        str.compare(pos, len, what) == 0 &&
        ends_word(str[pos+len]))
    {
      pos += len;
      return true;
    }
    return false;
  };

  auto addinfo = [&]() {
    skipwhite();
    if (pos >= size)
      return;
    auto keypos = pos;
    while (pos < size && str[pos] != '=' && str[pos] != c_isspace(str[pos]))
      ++pos;
    auto key = str.substr(keypos, pos-keypos);
    skipwhite();
    if (pos >= size || str[pos] != '=') {
      optconfig.Log(Error, "invalid entry in .PKGINFO");
      return;
    }
    ++pos;
    skipwhite();
    auto valuepos = pos;
    skipline();
    if (valuepos != pos) {
      auto value = str.substr(valuepos, pos-valuepos);
      this->info_[key].emplace_back(value);
    }
  };

  string es;
  string version, constraint;
  while (pos < size) {
    skipwhite();
    if (isentry("pkgname", sizeof("pkgname")-1)) {
      if (!getvalue("pkgname", name_))
        return false;
      continue;
    }
    if (isentry("pkgver", sizeof("pkgver")-1)) {
      if (!getvalue("pkgver", version_))
        return false;
      continue;
    }
    if (isentry("pkgbase", sizeof("pkgbase")-1)) {
      if (pkgbase_.empty()) {
        if (!getvalue("pkgbase", pkgbase_))
          return false;
        continue;
      }
      // if we already had a pkgbase line, fall through to the info_ case
    }

    if (!optconfig.package_depends_ && !optconfig.package_info_) {
      skipline();
      continue;
    }

    if (!optconfig.package_depends_) {
      addinfo();
      continue;
    }

    if (isentry("depend", sizeof("depend")-1)) {
      if (!getvalue("depend", es))
        return false;
      split_depstring(es, version, constraint);
      depends_.emplace_back(version, constraint);
      continue;
    }
    if (isentry("makedepend", sizeof("makedepend")-1)) {
      if (!getvalue("makedepend", es))
        return false;
      split_depstring(es, version, constraint);
      makedepends_.emplace_back(version, constraint);
      continue;
    }
    if (isentry("optdepend", sizeof("optdepend")-1)) {
      if (!getvalue("optdepend", es))
        return false;
      size_t c = es.find_first_of(':');
      if (c != string::npos)
        es.erase(c);
      if (es.length()) {
        split_depstring(es, version, constraint);
        optdepends_.emplace_back(version, constraint);
      }
      continue;
    }
    if (isentry("replace", sizeof("replaces")-1)) {
      if (!getvalue("replaces", es))
        return false;
      split_depstring(es, version, constraint);
      replaces_.emplace_back(version, constraint);
      continue;
    }
    if (isentry("conflict", sizeof("conflict")-1)) {
      if (!getvalue("conflict", es))
        return false;
      split_depstring(es, version, constraint);
      conflicts_.emplace_back(version, constraint);
      continue;
    }
    if (isentry("provides", sizeof("provides")-1)) {
      if (!getvalue("provides", es))
        return false;
      split_depstring(es, version, constraint);
      provides_.emplace_back(version, constraint);
      continue;
    }
    if (isentry("group", sizeof("group")-1)) {
      if (!getvalue("group", es))
        return false;
      groups_.insert(es);
      continue;
    }

    if (optconfig.package_info_)
      addinfo();
    else
      skipline();
  }
  return true;
}

static inline
tuple<string, string> splitpath(const string& path)
{
  size_t slash = path.find_last_of('/');
  if (slash == string::npos)
    return make_tuple("/", path);
  if (path[0] != '/')
    return make_tuple(move(string("/") + path.substr(0, slash)),
                           path.substr(slash+1));
  return make_tuple(path.substr(0, slash), path.substr(slash+1));
}

static bool read_object(Package         *pkg,
                        struct archive  *tar,
                        string         &&filename,
                        size_t           size,
                        const Config    &optconfig)
{
  vec<char> data;
  data.resize(size);

  ssize_t rc = archive_read_data(tar, &data[0], size);
  if (rc < 0) {
    optconfig.Log(Error, "failed to read from archive stream\n");
    return false;
  }
  else if ((size_t)rc != size) {
    optconfig.Log(Error, "file was short: %s\n", filename.c_str());
    return false;
  }

  bool err = false;
  rptr<Elf> object(Elf::Open(&data[0], data.size(), &err, filename.c_str(),
                             optconfig));
  if (!object.get()) {
    if (err)
      optconfig.Log(Error, "error in: %s\n", filename.c_str());
    return !err;
  }

  auto split(move(splitpath(filename)));
  object->dirname_  = move(std::get<0>(split));
  object->basename_ = move(std::get<1>(split));
  object->SolvePaths(object->dirname_);

  pkg->objects_.push_back(object);

  return true;
}

static bool add_entry(Package              *pkg,
                      struct archive       *tar,
                      struct archive_entry *entry,
                      const Config&         optconfig)
{
  string filename(archive_entry_pathname(entry));
  bool isinfo = filename == ".PKGINFO";

  mode_t mode = archive_entry_mode(entry);

  // one less string-copy:
  guard addfile([&filename,pkg]() {
    pkg->filelist_.emplace_back(move(filename));
  });
  if (!optconfig.package_filelist_ ||
      isinfo ||
      (AE_IFDIR == (mode & AE_IFDIR)) ||
      filename[filename.length()-1] == '/' ||
      filename == ".INSTALL" ||
      filename == ".MTREE")
  {
    addfile.release();
  }

  // for now we only care about files named lib.*\.so(\.|$)
  if (!isinfo && !care_about(entry, mode))
  {
    archive_read_data_skip(tar);
    return true;
  }

  if (AE_IFLNK == (mode & AE_IFLNK)) {
    // it's a symlink...
    const char *link = archive_entry_symlink(entry);
    if (!link) {
      optconfig.Log(Error, "error reading symlink");
      return false;
    }
    archive_read_data_skip(tar);
    pkg->load_.symlinks[filename] = link;
    return true;
  }

  // Check the size
  auto isize = archive_entry_size(entry);
  if (isize <= 0)
    return true;
  auto size = static_cast<size_t>(isize);

  if (isinfo)
    return read_info(pkg, tar, size, optconfig);

  return read_object(pkg, tar, move(filename), size, optconfig);
}

Elf* Package::Find(const string& dirname, const string& basename) const {
  for (auto &obj : objects_) {
    if (obj->dirname_ == dirname && obj->basename_ == basename)
      return const_cast<Elf*>(obj.get());
  }
  return nullptr;
}

void Package::Guess(const string& path) {
  // extract the basename:
  size_t at = path.find_last_of('/');
  string base(at == string::npos ? path : path.substr(at+1));

  // at least N.tgz
  if (base.length() < 5)
    return;

  // ArchLinux scheme:
  // ${name}-${pkgver}-${pkgrel}-${CARCH}.pkg.tar.*
  // Slackware:
  // ${name}-${pkgver}-${CARCH}-${build}.t{gz,bz2,xz}

  // so the first part up to the first /-\d/ is part of the name
  size_t to = base.find_first_of("-.");

  // sanity:
  if (!to || to == string::npos)
    return;

  while (to+1 < base.length() && // gonna check [to+1]
         base[to] != '.'      && // a dot ends the name
         !(base[to+1] >= '0' && base[to+1] <= '9'))
  {
    // name can have dashes, let's move to the next one
    to = base.find_first_of("-.", to+1);
  }

  name_ = base.substr(0, to);
  if (base[to] != '-' || !(base[to+1] >= '0' && base[to+1] <= '9')) {
    // no version
    return;
  }

  // version
  size_t from = to+1;
  to = base.find_first_of('-', from);

  if (to == string::npos) {
    // we'll take it...
    version_ = base.substr(from);
    return;
  }

  bool slack = true;

  // check for a pkgrel (Arch scheme)
  if (base[to] == '-' &&
      to+1 < base.length() &&
      (base[to+1] >= '0' && base[to+1] <= '9'))
  {
    slack = false;
    to = base.find_first_of("-.", to+1);
  }

  version_ = base.substr(from, to-from);
  if (!slack || to == string::npos)
    return;

  // slackware build-name comes right before the extension
  to = base.find_last_of('.');
  if (!to || to == string::npos)
    return;
  from = base.find_last_of("-.", to-1);
  if (from && from != string::npos) {
    version_.append(1, '-');
    version_.append(base.substr(from+1, to-from-1));
  }
}

Package* Package::Open(const string& path, const Config& optconfig) {
  uniq<Package> package(new Package);

  struct archive *tar = archive_read_new();
  archive_read_support_filter_all(tar);
  archive_read_support_format_all(tar);

  struct archive_entry *entry;
  if (ARCHIVE_OK != archive_read_open_filename(tar, path.c_str(), 10240)) {
    return 0;
  }

  while (ARCHIVE_OK == archive_read_next_header(tar, &entry)) {
    if (!add_entry(package.get(), tar, entry, optconfig))
      return 0;
  }

  archive_read_free(tar);

  if (!package->name_.length() && !package->version_.length())
    package->Guess(path);

  bool changed;
  do {
    changed = false;
    for (auto link = package->load_.symlinks.begin(); link != package->load_.symlinks.end();)
    {
      auto linkfrom = splitpath(link->first);
      decltype(linkfrom) linkto;

      // handle relative as well as absolute symlinks
      if (!link->second.length()) {
        // illegal
        ++link;
        continue;
      }
      if (link->second[0] == '/') // absolute
        linkto = splitpath(link->second);
      else // relative
      {
        string fullpath = std::get<0>(linkfrom) + "/" + link->second;
        linkto = splitpath(fullpath);
      }

      Elf *obj = package->Find(std::get<0>(linkto), std::get<1>(linkto));
      if (!obj) {
        ++link;
        continue;
      }
      changed = true;

      Elf *copy = new Elf(*obj);
      copy->dirname_  = move(std::get<0>(linkfrom));
      copy->basename_ = move(std::get<1>(linkfrom));
      copy->SolvePaths(obj->dirname_);

      package->objects_.push_back(copy);
      package->load_.symlinks.erase(link++);
    }
  } while (changed);
  package->load_.symlinks.clear();

  return package.release();
}

void Package::ShowNeeded() {
  const char *name = this->name_.c_str();
  for (auto &obj : objects_) {
    string path = obj->dirname_ + "/" + obj->basename_;
    const char *objname = path.c_str();
    for (auto &need : obj->needed_) {
      printf("%s: %s NEEDS %s\n", name, objname, need.c_str());
    }
  }
}

bool Package::ConflictsWith(const Package &other) const {
  for (auto &conf : conflicts_) {
    const string &name = std::get<0>(conf);
#ifdef PKGDEPDB_ENABLE_ALPM
    string op, ver;
    split_constraint(std::get<1>(conf), op, ver);
    if (ver.length()) {
      if (package_satisfies(&other, name, op, ver))
        return true;
    } else {
#endif
      if (other.name_ == name)
        return true;
      for (auto &prov : other.provides_) {
        if (std::get<0>(prov) == name)
          return true;
      }
#ifdef PKGDEPDB_ENABLE_ALPM
    }
#endif
  }
  return false;
}

bool Package::Replaces(const Package &other) const {
  for (auto &conf : replaces_) {
    const string &name = std::get<0>(conf);
#ifdef PKGDEPDB_ENABLE_ALPM
    string op, ver;
    split_constraint(std::get<1>(conf), op, ver);
    if (ver.length()) {
      if (package_satisfies(&other, name, op, ver))
        return true;
    } else {
#endif
      if (other.name_ == name)
        return true;
      for (auto &prov : other.provides_) {
        if (std::get<0>(prov) == name)
          return true;
      }
#ifdef PKGDEPDB_ENABLE_ALPM
    }
#endif
  }
  return false;
}

} // ::pkgdepdb
