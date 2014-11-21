#include <errno.h>
#include <string.h>

#include <fstream>

#include "main.h"
#include "elf.h"
#include "package.h"
#include "db.h"

#include "pkgdepdb.h"

#include "capi_algorithm.h"

using namespace pkgdepdb;

extern "C" {

pkgdepdb_elf pkgdepdb_elf_new() {
  auto out = new rptr<Elf>(new Elf);
  return reinterpret_cast<pkgdepdb_elf>(out);
}

void pkgdepdb_elf_unref(pkgdepdb_elf elf_) {
  auto elf = reinterpret_cast<rptr<Elf>*>(elf_);
  delete elf;
}

pkgdepdb_elf pkgdepdb_elf_load(const char *file, int *err,
                               pkgdepdb_cfg *cfg_)
{
  if (err)
    *err = 0;
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    pkgdepdb_set_error("open failed: %s", strerror(errno));
    return nullptr;
  }
  if (!in.seekg(0, std::ios::end)) {
    pkgdepdb_set_error("seek failed: %s", strerror(errno));
    return nullptr;
  }
  auto size = in.tellg();
  if (!in.seekg(0, std::ios::beg)) {
    pkgdepdb_set_error("seek failed: %s", strerror(errno));
    return nullptr;
  }

  auto data = new char[size];
  if (!in.read((char*)data, size)) {
    pkgdepdb_set_error("read failed: %s", strerror(errno));
    delete[] data;
    return nullptr;
  }
  in.close();

  bool waserror = false;
  auto cfg = reinterpret_cast<Config*>(cfg_);
  auto out = new rptr<Elf>(Elf::Open(data, size, &waserror, file, *cfg));
  delete[] data;

  if (!*out) {
    delete out;
    if (err) {
      pkgdepdb_set_error("error parsing ELF data");
      *err = waserror;
    }
    return nullptr;
  }

  string path(file);
  auto   slash = path.find_last_of('/', path.length());
  if (slash == string::npos) {
    (*out)->basename_ = move(path);
    (*out)->dirname_.clear();
  } else {
    (*out)->basename_ = path.substr(slash+1);
    (*out)->dirname_ = path.substr(0, slash);
  }

  return reinterpret_cast<pkgdepdb_elf>(out);
}

pkgdepdb_elf pkgdepdb_elf_read(const char *data, size_t size,
                               const char *basename, const char *dirname,
                               int *err, pkgdepdb_cfg *cfg_)
{
  if (err)
    *err = 0;

  auto cfg = reinterpret_cast<Config*>(cfg_);
  bool waserror = false;
  auto out = new rptr<Elf>(Elf::Open(data, size, &waserror, basename, *cfg));

  if (!*out) {
    delete out;
    if (err)
      *err = waserror;
    return nullptr;
  }

  (*out)->basename_ = basename;
  (*out)->dirname_  = dirname;

  return reinterpret_cast<pkgdepdb_elf>(out);
}

const char* pkgdepdb_elf_dirname(pkgdepdb_elf elf_) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  return elf->dirname_.c_str();
}

const char* pkgdepdb_elf_basename(pkgdepdb_elf elf_) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  return elf->basename_.c_str();
}

void pkgdepdb_elf_set_dirname(pkgdepdb_elf elf_, const char *v) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  elf->dirname_ = v;
}

void pkgdepdb_elf_set_basename(pkgdepdb_elf elf_, const char *v) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  elf->basename_ = v;
}

unsigned char pkgdepdb_elf_class(pkgdepdb_elf elf_) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  return elf->ei_class_;
}

unsigned char pkgdepdb_elf_data(pkgdepdb_elf elf_) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  return elf->ei_data_;
}

unsigned char pkgdepdb_elf_osabi(pkgdepdb_elf elf_) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  return elf->ei_osabi_;
}

void pkgdepdb_elf_set_class(pkgdepdb_elf elf_, unsigned char v) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  elf->ei_class_ = v;
}

void pkgdepdb_elf_set_data(pkgdepdb_elf elf_, unsigned char v) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  elf->ei_data_ = v;
}

void pkgdepdb_elf_set_osabi(pkgdepdb_elf elf_, unsigned char v) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  elf->ei_osabi_ = v;
}

const char* pkgdepdb_elf_class_string(pkgdepdb_elf elf_) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  return elf->classString();
}

const char* pkgdepdb_elf_data_string(pkgdepdb_elf elf_) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  return elf->dataString();
}

const char* pkgdepdb_elf_osabi_string(pkgdepdb_elf elf_) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  return elf->osabiString();
}

const char* pkgdepdb_elf_rpath(pkgdepdb_elf elf_) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  return elf->rpath_set_ ? elf->rpath_.c_str()
                         : nullptr;
}

const char* pkgdepdb_elf_runpath(pkgdepdb_elf elf_) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  return elf->runpath_set_ ? elf->runpath_.c_str()
                           : nullptr;
}

const char* pkgdepdb_elf_interpreter(pkgdepdb_elf elf_) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  return elf->interpreter_set_ ? elf->interpreter_.c_str()
                               : nullptr;
}

void pkgdepdb_elf_set_rpath(pkgdepdb_elf elf_, const char *v) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  if ( (elf->rpath_set_ = !!v) )
    elf->rpath_ = v;
  else
    elf->rpath_.clear();
}

void pkgdepdb_elf_set_runpath(pkgdepdb_elf elf_, const char *v) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  if ( (elf->runpath_set_ = !!v) )
    elf->runpath_ = v;
  else
    elf->runpath_.clear();
}

void pkgdepdb_elf_set_interpreter(pkgdepdb_elf elf_, const char *v) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  if ( (elf->interpreter_set_ = !!v) )
    elf->interpreter_ = v;
  else
    elf->interpreter_.clear();
}

size_t pkgdepdb_elf_needed_count(pkgdepdb_elf elf_) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  return elf->needed_.size();
}

size_t pkgdepdb_elf_needed_get(pkgdepdb_elf elf_, const char **out, size_t off,
                               size_t count)
{
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  return pkgdepdb_strlist_get(*elf, &Elf::needed_, out, off, count);
}

void pkgdepdb_elf_needed_add(pkgdepdb_elf elf_, const char *v) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  elf->needed_.emplace_back(v);
}

void pkgdepdb_elf_needed_insert(pkgdepdb_elf elf_, size_t index, const char *v)
{
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  elf->needed_.insert(elf->needed_.begin() + index, v);
}

void pkgdepdb_elf_needed_insert_r(pkgdepdb_elf elf_, size_t index, size_t count,
                                  const char **v)
{
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  elf->needed_.insert(elf->needed_.begin() + index, v, v + count);
}

pkgdepdb_bool pkgdepdb_elf_needed_contains(pkgdepdb_elf elf_, const char *v) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  return pkgdepdb_strlist_contains(elf->needed_, v);
}

size_t pkgdepdb_elf_needed_del_s(pkgdepdb_elf elf_, const char *v) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  return pkgdepdb_strlist_del_s_all(elf->needed_, v);
}

void pkgdepdb_elf_needed_del_i(pkgdepdb_elf elf_, size_t index) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  elf->needed_.erase(elf->needed_.begin() + index);
}

void pkgdepdb_elf_needed_del_r(pkgdepdb_elf elf_, size_t index, size_t count) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  elf->needed_.erase(elf->needed_.begin() + index,
                     elf->needed_.begin() + index + count);
}

pkgdepdb_bool pkgdepdb_elf_can_use(pkgdepdb_elf subject, pkgdepdb_elf object,
                                   pkgdepdb_bool strict)
{
  auto elf = *reinterpret_cast<rptr<Elf>*>(subject);
  auto other = *reinterpret_cast<rptr<Elf>*>(object);
  return elf->CanUse(*other, strict);
}

size_t pkgdepdb_elf_missing_count(pkgdepdb_elf elf_) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  return elf->req_missing_.size();
}

size_t pkgdepdb_elf_missing_get(pkgdepdb_elf elf_, const char **out,
                                size_t off, size_t count)
{
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  return pkgdepdb_strlist_get(elf->req_missing_, out, off, count);
}

pkgdepdb_bool pkgdepdb_elf_missing_contains(pkgdepdb_elf elf_, const char *v) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  return pkgdepdb_strlist_contains(elf->req_missing_, v);
}

size_t pkgdepdb_elf_found_count(pkgdepdb_elf elf_) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  return elf->req_found_.size();
}

size_t pkgdepdb_elf_found_get(pkgdepdb_elf elf_, pkgdepdb_elf *out, size_t off,
                              size_t count)
{
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  auto len = elf->req_found_.size();
  if (off >= len)
    return 0;
  auto i   = elf->req_found_.begin();
  auto end = elf->req_found_.begin();
  size_t got = 0;
  while (off--)
    ++i;
  for (; got < count && i != end; ++i) {
    rptr<Elf> **dest = reinterpret_cast<rptr<Elf>**>(&out[got++]);
    if (*dest)
      **dest = *i;
    else
      *dest = new rptr<Elf>(*i);
  }
  return got;
}

pkgdepdb_elf pkgdepdb_elf_found_find(pkgdepdb_elf elf_, const char *name) {
  auto elf = *reinterpret_cast<rptr<Elf>*>(elf_);
  for (Elf *obj : elf->req_found_)
    if (obj->basename_ == name)
      return reinterpret_cast<pkgdepdb_elf>(new rptr<Elf>(obj));
  return nullptr;
}

} // extern "C"
