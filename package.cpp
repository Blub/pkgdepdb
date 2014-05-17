#include <memory>

#include <archive.h>
#include <archive_entry.h>

#include "main.h"

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

// NOTE:
// Judgding from pacman/libalpm source code this function
// is way less strict about the formatting, as we skip whitespace
// between every word, whereas pacman matches /^(\w+) = (.*)$/ exactly.
static bool read_info(Package *pkg, struct archive *tar, const size_t size) {
  std::vector<char> data(size);
  ssize_t rc = archive_read_data(tar, &data[0], size);
  if ((size_t)rc != size) {
    log(Error, "failed to read .PKGINFO");
    return false;
  }

  std::string str(&data[0], data.size());

  size_t pos = 0;
  auto skipwhite = [&]() {
    while (pos < size && c_isspace(str[pos]))
      ++pos;
  };

  auto skipline = [&]() {
    while (pos < size && str[pos] != '\n')
      ++pos;
  };

  auto getvalue = [&](const char *entryname, std::string &out) -> bool {
    skipwhite();
    if (pos >= size) {
      log(Error, "invalid %s entry in .PKGINFO", entryname);
      return false;
    }
    if (str[pos] != '=') {
      log(Error, "Error in .PKGINFO");
      return false;
    }
    ++pos;
    skipwhite();
    if (pos >= size) {
      log(Error, "invalid %s entry in .PKGINFO", entryname);
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

  std::string es;
  while (pos < size) {
    skipwhite();
    if (isentry("pkgname", sizeof("pkgname")-1)) {
      if (!getvalue("pkgname", pkg->name_))
        return false;
      continue;
    }
    if (isentry("pkgver", sizeof("pkgver")-1)) {
      if (!getvalue("pkgver", pkg->version_))
        return false;
      continue;
    }

    if (!opt_package_depends) {
      skipline();
      continue;
    }

    if (isentry("depend", sizeof("depend")-1)) {
      if (!getvalue("depend", es))
        return false;
      pkg->depends_.push_back(es);
      continue;
    }
    if (isentry("optdepend", sizeof("optdepend")-1)) {
      if (!getvalue("optdepend", es))
        return false;
      size_t c = es.find_first_of(':');
      if (c != std::string::npos)
        es.erase(c);
      if (es.length())
        pkg->optdepends_.push_back(es);
      continue;
    }
    if (isentry("replace", sizeof("replaces")-1)) {
      if (!getvalue("replaces", es))
        return false;
      pkg->replaces_.push_back(es);
      continue;
    }
    if (isentry("conflict", sizeof("conflict")-1)) {
      if (!getvalue("conflict", es))
        return false;
      pkg->conflicts_.push_back(es);
      continue;
    }
    if (isentry("provides", sizeof("provides")-1)) {
      if (!getvalue("provides", es))
        return false;
      pkg->provides_.push_back(es);
      continue;
    }
    if (isentry("group", sizeof("group")-1)) {
      if (!getvalue("group", es))
        return false;
      pkg->groups_.insert(es);
      continue;
    }

    skipline();
  }
  return true;
}

static inline
std::tuple<std::string, std::string> splitpath(const std::string& path)
{
  size_t slash = path.find_last_of('/');
  if (slash == std::string::npos)
    return std::make_tuple("/", path);
  if (path[0] != '/')
    return std::make_tuple(std::move(std::string("/") + path.substr(0, slash)),
                           path.substr(slash+1));
  return std::make_tuple(path.substr(0, slash), path.substr(slash+1));
}

static bool read_object(Package         *pkg,
                        struct archive  *tar,
                        std::string    &&filename,
                        size_t           size)
{
  std::vector<char> data;
  data.resize(size);

  ssize_t rc = archive_read_data(tar, &data[0], size);
  if (rc < 0) {
    log(Error, "failed to read from archive stream\n");
    return false;
  }
  else if ((size_t)rc != size) {
    log(Error, "file was short: %s\n", filename.c_str());
    return false;
  }

  bool err = false;
  rptr<Elf> object(Elf::Open(&data[0], data.size(), &err, filename.c_str()));
  if (!object.get()) {
    if (err)
      log(Error, "error in: %s\n", filename.c_str());
    return !err;
  }

  auto split(std::move(splitpath(filename)));
  object->dirname_  = std::move(std::get<0>(split));
  object->basename_ = std::move(std::get<1>(split));
  object->SolvePaths(object->dirname_);

  pkg->objects_.push_back(object);

  return true;
}

static bool add_entry(Package              *pkg,
                      struct archive       *tar,
                      struct archive_entry *entry)
{
  std::string filename(archive_entry_pathname(entry));
  bool isinfo = filename == ".PKGINFO";

  mode_t mode = archive_entry_mode(entry);

  // one less string-copy:
  guard addfile([&filename,pkg]() {
    pkg->filelist_.emplace_back(std::move(filename));
  });
  if (!opt_package_filelist ||
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
      log(Error, "error reading symlink");
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
    return read_info(pkg, tar, size);

  return read_object(pkg, tar, std::move(filename), size);
}

Elf* Package::Find(const std::string& dirname,
                   const std::string& basename) const
{
  for (auto &obj : objects_) {
    if (obj->dirname_ == dirname && obj->basename_ == basename)
      return const_cast<Elf*>(obj.get());
  }
  return nullptr;
}

void Package::Guess(const std::string& path) {
  // extract the basename:
  size_t at = path.find_last_of('/');
  std::string base(at == std::string::npos ? path : path.substr(at+1));

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
  if (!to || to == std::string::npos)
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

  if (to == std::string::npos) {
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
  if (!slack || to == std::string::npos)
    return;

  // slackware build-name comes right before the extension
  to = base.find_last_of('.');
  if (!to || to == std::string::npos)
    return;
  from = base.find_last_of("-.", to-1);
  if (from && from != std::string::npos) {
    version_.append(1, '-');
    version_.append(base.substr(from+1, to-from-1));
  }
}

Package* Package::Open(const std::string& path) {
  std::unique_ptr<Package> package(new Package);

  struct archive *tar = archive_read_new();
  archive_read_support_filter_all(tar);
  archive_read_support_format_all(tar);

  struct archive_entry *entry;
  if (ARCHIVE_OK != archive_read_open_filename(tar, path.c_str(), 10240)) {
    return 0;
  }

  while (ARCHIVE_OK == archive_read_next_header(tar, &entry)) {
    if (!add_entry(package.get(), tar, entry))
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
        std::string fullpath = std::get<0>(linkfrom) + "/" + link->second;
        linkto = splitpath(fullpath);
      }

      Elf *obj = package->Find(std::get<0>(linkto), std::get<1>(linkto));
      if (!obj) {
        ++link;
        continue;
      }
      changed = true;

      Elf *copy = new Elf(*obj);
      copy->dirname_  = std::move(std::get<0>(linkfrom));
      copy->basename_ = std::move(std::get<1>(linkfrom));
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
    std::string path = obj->dirname_ + "/" + obj->basename_;
    const char *objname = path.c_str();
    for (auto &need : obj->needed_) {
      printf("%s: %s NEEDS %s\n", name, objname, need.c_str());
    }
  }
}

bool Package::ConflictsWith(const Package &other) const {
  for (auto &conf : conflicts_) {
#ifdef WITH_ALPM
    std::string name, op, ver;
    split_depstring(conf, name, op, ver);
    if (ver.length()) {
      if (package_satisfies(&other, name, op, ver))
        return true;
    } else {
#else
      std::string &name(conf);
#endif
      if (other.name_ == name)
        return true;
      for (auto &prov : other.provides_) {
#ifdef WITH_ALPM
        std::string provname;
        split_depstring(prov, provname, op, ver);
#else
        std::string &provname(prov);
#endif
        if (provname == name)
          return true;
      }
#ifdef WITH_ALPM
    }
#endif
  }
  return false;
}

bool Package::Replaces(const Package &other) const {
  for (auto &conf : replaces_) {
#ifdef WITH_ALPM
    std::string name, op, ver;
    split_depstring(conf, name, op, ver);
    if (ver.length()) {
      if (package_satisfies(&other, name, op, ver))
        return true;
    } else {
#else
      std::string &name(conf);
#endif
      if (other.name_ == name)
        return true;
      for (auto &prov : other.provides_) {
#ifdef WITH_ALPM
        std::string provname;
        split_depstring(prov, provname, op, ver);
#else
        std::string &provname(prov);
#endif
        if (provname == name)
          return true;
      }
#ifdef WITH_ALPM
    }
#endif
  }
  return false;
}
