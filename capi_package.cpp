#include <algorithm>
#include <fstream>

#include "main.h"
#include "elf.h"
#include "package.h"
#include "db.h"

#include "pkgdepdb.h"

#include "capi_algorithm.h"

using namespace pkgdepdb;

extern "C" {

pkgdepdb_pkg* pkgdepdb_pkg_new(void) {
  return reinterpret_cast<pkgdepdb_pkg*>(new Package);
}

pkgdepdb_pkg* pkgdepdb_pkg_load(const char *filename, pkgdepdb_cfg *cfg_) {
  auto cfg = reinterpret_cast<Config*>(cfg_);
  auto pkg = Package::Open(filename, *cfg);
  return reinterpret_cast<pkgdepdb_pkg*>(pkg);
}

void pkgdepdb_pkg_delete(pkgdepdb_pkg *pkg_) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  delete pkg;
}

const char* pkgdepdb_pkg_name(pkgdepdb_pkg *pkg_) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkg->name_.c_str();
}

const char* pkgdepdb_pkg_version(pkgdepdb_pkg *pkg_) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkg->version_.c_str();
}

const char* pkgdepdb_pkg_pkgbase(pkgdepdb_pkg *pkg_) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkg->pkgbase_.c_str();
}

const char* pkgdepdb_pkg_description(pkgdepdb_pkg *pkg_) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkg->description_.c_str();
}

void pkgdepdb_pkg_set_name(pkgdepdb_pkg *pkg_, const char *v) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  pkg->name_ = v ? v : "";
}

void pkgdepdb_pkg_set_version(pkgdepdb_pkg *pkg_, const char *v) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  pkg->version_ = v ? v : "";
}

void pkgdepdb_pkg_set_pkgbase(pkgdepdb_pkg *pkg_, const char *v) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  pkg->pkgbase_ = v ? v : "";
}

void pkgdepdb_pkg_set_description(pkgdepdb_pkg *pkg_, const char *v) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  pkg->description_ = v ? v : "";
}

pkgdepdb_bool pkgdepdb_pkg_read_info(pkgdepdb_pkg *pkg_, const char *data,
                                     size_t size, pkgdepdb_cfg *cfg_)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  auto cfg = reinterpret_cast<Config*>(cfg_);
  return pkg->ReadInfo(string(data, size), size, *cfg);
}

static DependList Package::* const kDepMember[] = {
  &Package::depends_,
  &Package::optdepends_,
  &Package::makedepends_,
  &Package::checkdepends_,
  &Package::provides_,
  &Package::conflicts_,
  &Package::replaces_,
};
static const size_t kDepMemberCount = sizeof(kDepMember)/sizeof(kDepMember[0]);

static inline
size_t pkg_dependlist_count(const Package& pkg, DependList Package::*member) {
  return (pkg.*member).size();
}

static inline
size_t pkg_dependlist_get(const Package& pkg, DependList Package::*member,
                          const char **on, const char **oc, size_t off,
                          size_t n)
{
  if (off >= (pkg.*member).size())
    return 0;
  auto i   = (pkg.*member).begin() + off;
  auto end = (pkg.*member).end();
  size_t got = 0;
  for (; i != end; ++i) {
    if (!n--)
      return got;
    on[got] = std::get<0>(*i).c_str();
    oc[got] = std::get<1>(*i).c_str();
    ++got;
  }
  return got;
}

static inline
pkgdepdb_bool pkg_dependlist_add(Package& pkg, DependList Package::*member,
                                 const char *name, const char *constraint)
{
  (pkg.*member).emplace_back(name, constraint ? constraint : "");
  return 1;
}

static inline
pkgdepdb_bool pkg_dependlist_insert(Package& pkg, DependList Package::*member,
                                    size_t index, size_t count,
                                    const char **names, const char **constrs)
{
  auto& mem = pkg.*member;
  const auto size = mem.size();
  mem.resize(size + count);
  // move the tail
  const auto tailsize = size - index;
  for (size_t i = 0; i != tailsize; ++i) {
    mem[size-1 + count - i] = move(mem[size-1 - i]);
  }
  for (size_t i = 0; i != count; ++i) {
    std::get<0>(mem[index+i]) = names[i];
    std::get<1>(mem[index+i]) = constrs[i];
  }
  return 1;
}

static inline
pkgdepdb_bool pkg_dependlist_contains(Package& pkg,
                                      DependList Package::*member,
                                      const char *name)
{
  for (const auto& dep : pkg.*member)
    if (std::get<0>(dep) == name)
      return 1;
  return 0;
}

static inline
int pkg_dependlist_del_name(Package& pkg, DependList Package::*member,
                            const char *name)
{
  auto i = (pkg.*member).begin();
  auto end = (pkg.*member).end();
  for (; i != end; ++i) {
    if (std::get<0>(*i) == name) {
      (pkg.*member).erase(i);
      return 1;
    }
  }
  return 0;
}

static inline
int pkg_dependlist_del_full(Package& pkg, DependList Package::*member,
                            const char *name, const char *constraint)
{
  auto i = (pkg.*member).begin();
  auto end = (pkg.*member).end();
  for (; i != end; ++i) {
    if (std::get<0>(*i) == name && std::get<1>(*i) == constraint) {
      (pkg.*member).erase(i);
      return 1;
    }
  }
  return 0;
}

static inline
int pkg_dependlist_del_i(Package& pkg, DependList Package::*member, size_t i) {
  if (i >= (pkg.*member).size())
    return 0;
  (pkg.*member).erase((pkg.*member).begin() + i);
  return 1;
}

static inline
size_t pkg_dependlist_del_r(Package& pkg, DependList Package::*member,
                            size_t i, size_t count)
{
  return pkgdepdb_strlist_del_r(pkg.*member, i, count);
}

size_t pkgdepdb_pkg_dep_count(pkgdepdb_pkg *pkg_, unsigned int what) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  if (what >= kDepMemberCount)
    return 0;
  return pkg_dependlist_count(*pkg, kDepMember[what]);
}

size_t pkgdepdb_pkg_dep_get(pkgdepdb_pkg *pkg_, unsigned int what,
                            const char **on, const char **oc, size_t off,
                            size_t n)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  if (what >= kDepMemberCount)
    return 0;
  return pkg_dependlist_get(*pkg, kDepMember[what], on, oc, off, n);
}

pkgdepdb_bool pkgdepdb_pkg_dep_add(pkgdepdb_pkg *pkg_, unsigned int what,
                                   const char *name, const char *constraint)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  if (what >= kDepMemberCount)
    return 0;
  return pkg_dependlist_add(*pkg, kDepMember[what], name, constraint);
}

pkgdepdb_bool pkgdepdb_pkg_dep_insert(pkgdepdb_pkg *pkg_, unsigned int what,
                                      size_t index,
                                      const char *name, const char *constr)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  if (what >= kDepMemberCount)
    return 0;
  return pkg_dependlist_insert(*pkg, kDepMember[what],
                               index, 1, &name, &constr);
}

size_t pkgdepdb_pkg_dep_insert_r(pkgdepdb_pkg *pkg_, unsigned int what,
                                 size_t index, size_t count,
                                 const char **names, const char **cnts)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  if (what >= kDepMemberCount)
    return 0;
  return pkg_dependlist_insert(*pkg, kDepMember[what],
                               index, count, names, cnts);
}

pkgdepdb_bool pkgdepdb_pkg_dep_contains(pkgdepdb_pkg *pkg_, unsigned int what,
                                        const char *name)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  if (what >= kDepMemberCount)
    return 0;
  return pkg_dependlist_contains(*pkg, kDepMember[what], name);
}

size_t pkgdepdb_pkg_dep_del_name(pkgdepdb_pkg *pkg_, unsigned int what,
                                 const char *name)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  if (what >= kDepMemberCount)
    return 0;
  return pkg_dependlist_del_name(*pkg, kDepMember[what], name);
}

size_t pkgdepdb_pkg_dep_del_full(pkgdepdb_pkg *pkg_, unsigned int what,
                                 const char *name, const char *constraint)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  if (what >= kDepMemberCount)
    return 0;
  return pkg_dependlist_del_full(*pkg, kDepMember[what], name, constraint);
}

size_t pkgdepdb_pkg_dep_del_i(pkgdepdb_pkg *pkg_, unsigned int what, size_t id)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  if (what >= kDepMemberCount)
    return 0;
  return pkg_dependlist_del_i(*pkg, kDepMember[what], id);
}

size_t pkgdepdb_pkg_dep_del_r(pkgdepdb_pkg *pkg_, unsigned int what, size_t id,
                              size_t count)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  if (what >= kDepMemberCount)
    return 0;
  return pkg_dependlist_del_r(*pkg, kDepMember[what], id, count);
}

size_t pkgdepdb_pkg_groups_count(pkgdepdb_pkg *pkg_) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkgdepdb_strlist_count(*pkg, &Package::groups_);
}

size_t pkgdepdb_pkg_groups_get(pkgdepdb_pkg *pkg_, const char **out,
                               size_t off, size_t count) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkgdepdb_strlist_get(*pkg, &Package::groups_, out, off, count);
}

size_t pkgdepdb_pkg_groups_add(pkgdepdb_pkg *pkg_, const char *v) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkgdepdb_strlist_add_unique(*pkg, &Package::groups_, v);
}

size_t pkgdepdb_pkg_groups_add_r(pkgdepdb_pkg *pkg_, size_t count,
                                 const char **v)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkgdepdb_strlist_insert_unique(*pkg, &Package::groups_, count, v);
}

pkgdepdb_bool pkgdepdb_pkg_groups_contains(pkgdepdb_pkg *pkg_, const char *v) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkgdepdb_strlist_contains(pkg->groups_, v);
}

size_t pkgdepdb_pkg_groups_del_s(pkgdepdb_pkg *pkg_, const char *v) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkgdepdb_strlist_del_s_all(*pkg, &Package::groups_, v);
}

size_t pkgdepdb_pkg_groups_del_i(pkgdepdb_pkg *pkg_, size_t index) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkgdepdb_strlist_del_i(*pkg, &Package::groups_, index);
}

size_t pkgdepdb_pkg_groups_del_r(pkgdepdb_pkg *pkg_, size_t index, size_t cnt)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkgdepdb_strlist_del_r(pkg->groups_, index, cnt);
}

size_t pkgdepdb_pkg_filelist_count(pkgdepdb_pkg *pkg_) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkgdepdb_strlist_count(*pkg, &Package::filelist_);
}

size_t pkgdepdb_pkg_filelist_get(pkgdepdb_pkg *pkg_,
                                 const char **out, size_t off, size_t count)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkgdepdb_strlist_get(*pkg, &Package::filelist_, out, off, count);
}

size_t pkgdepdb_pkg_filelist_add(pkgdepdb_pkg *pkg_, const char *v) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkgdepdb_strlist_add_always(*pkg, &Package::filelist_, v);
}

size_t pkgdepdb_pkg_filelist_insert(pkgdepdb_pkg *pkg_, size_t index,
                                    const char *v)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkgdepdb_strlist_insert_always(*pkg, &Package::filelist_, index, 1,
                                        &v);
}

size_t pkgdepdb_pkg_filelist_insert_r(pkgdepdb_pkg *pkg_, size_t index,
                                      size_t count, const char **v)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkgdepdb_strlist_insert_always(*pkg, &Package::filelist_, index,
                                        count, v);
}

pkgdepdb_bool pkgdepdb_pkg_filelist_contains(pkgdepdb_pkg *pkg_, const char *v)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkgdepdb_strlist_contains(pkg->filelist_, v);
}

size_t pkgdepdb_pkg_filelist_del_s(pkgdepdb_pkg *pkg_, const char *v) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkgdepdb_strlist_del_s_one(*pkg, &Package::filelist_, v);
}

size_t pkgdepdb_pkg_filelist_del_i(pkgdepdb_pkg *pkg_, size_t index) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkgdepdb_strlist_del_i(*pkg, &Package::filelist_, index);
}

size_t pkgdepdb_pkg_filelist_del_r(pkgdepdb_pkg *pkg_, size_t idx, size_t cnt)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkgdepdb_strlist_del_r(pkg->filelist_, idx, cnt);
}

pkgdepdb_bool pkgdepdb_pkg_filelist_set_i(pkgdepdb_pkg *pkg_, size_t index,
                                          const char *v)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkgdepdb_strlist_set_i(pkg->filelist_, index, v);
}

void pkgdepdb_pkg_guess(pkgdepdb_pkg *pkg_, const char *filename) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  pkg->Guess(filename);
}

pkgdepdb_bool pkgdepdb_pkg_conflict(pkgdepdb_pkg *subj_, pkgdepdb_pkg *obj_) {
  auto pkg = reinterpret_cast<Package*>(subj_);
  auto obj = reinterpret_cast<Package*>(obj_);
  return pkg->ConflictsWith(*obj);
}

pkgdepdb_bool pkgdepdb_pkg_replaces(pkgdepdb_pkg *subj_, pkgdepdb_pkg *obj_) {
  auto pkg = reinterpret_cast<Package*>(subj_);
  auto obj = reinterpret_cast<Package*>(obj_);
  return pkg->Replaces(*obj);
}

size_t pkgdepdb_pkg_elf_count(pkgdepdb_pkg *pkg_) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkg->objects_.size();
}

size_t pkgdepdb_pkg_elf_get(pkgdepdb_pkg *pkg_, pkgdepdb_elf *out,
                            size_t off, size_t count)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  if (off >= pkg->objects_.size())
    return 0;
  auto i   = pkg->objects_.begin() + off;
  auto end = pkg->objects_.end();
  size_t got = 0;
  for (; i != end; ++i) {
    rptr<Elf> **dest;
    if (!count--)
      return got;
    dest = reinterpret_cast<rptr<Elf>**>(&out[got++]);
    if (*dest)
      **dest = *i;
    else
      *dest = new rptr<Elf>(*i);
  }
  return got;
}

size_t pkgdepdb_pkg_elf_add(pkgdepdb_pkg *pkg_, pkgdepdb_elf elf_) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  auto elf = reinterpret_cast<rptr<Elf>*>(elf_);
  if (!elf || !*elf)
    return 0;
  pkg->objects_.push_back(*elf);
  return 1;
}

size_t pkgdepdb_pkg_elf_insert(pkgdepdb_pkg *pkg_, size_t index,
                               pkgdepdb_elf elf_)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  auto elf = reinterpret_cast<rptr<Elf>*>(elf_);
  if (!elf || !*elf)
    return 0;
  pkg->objects_.insert(pkg->objects_.begin() + index, *elf);
  return 1;
}

size_t pkgdepdb_pkg_elf_insert_r(pkgdepdb_pkg *pkg_, size_t index,
                                 size_t count, pkgdepdb_elf *elf_)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  auto elf = reinterpret_cast<rptr<Elf>*>(elf_);
  if (!elf || !*elf)
    return 0;
  pkg->objects_.insert(pkg->objects_.begin() + index, elf, elf + count);
  return 1;
}

size_t pkgdepdb_pkg_elf_del_e(pkgdepdb_pkg *pkg_, pkgdepdb_elf elf_) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  auto elf = reinterpret_cast<rptr<Elf>*>(elf_);
  auto it  = std::find(pkg->objects_.begin(), pkg->objects_.end(), *elf);
  if (it == pkg->objects_.end())
    return 0;
  pkg->objects_.erase(it);
  return 1;
}

size_t pkgdepdb_pkg_elf_del_i(pkgdepdb_pkg *pkg_, size_t index) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  if (index >= pkg->objects_.size())
    return 0;
  pkg->objects_.erase(pkg->objects_.begin() + index);
  return 1;
}

size_t pkgdepdb_pkg_elf_del_r(pkgdepdb_pkg *pkg_, size_t index, size_t count) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  size_t max = pkg->objects_.size();
  if (index >= max)
    return 0;
  if (count > max - index)
    count = max - index;
  pkg->objects_.erase(pkg->objects_.begin() + index,
                      pkg->objects_.begin() + index + count);
  return 1;
}

int pkgdepdb_pkg_elf_set_i(pkgdepdb_pkg *pkg_, size_t index,
                           pkgdepdb_elf elf_)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  auto elf = reinterpret_cast<rptr<Elf>*>(elf_);
  if (index >= pkg->objects_.size())
    return 0;
  if (!elf || !*elf) {
    pkg->objects_.erase(pkg->objects_.begin() + index);
    return -1;
  }
  pkg->objects_[index] = *elf;
  return 1;
}

size_t pkgdepdb_pkg_info_count_keys(pkgdepdb_pkg *pkg_) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkg->info_.size();
}

size_t pkgdepdb_pkg_info_get_keys(pkgdepdb_pkg *pkg_, const char **out,
                                  size_t off, size_t count)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  auto size = pkg->info_.size();
  if (off >= size)
    return 0;

  auto i    = pkg->info_.begin();
  auto end  = pkg->info_.end();

  while (off--)
    ++i;

  size_t got = 0;
  for (; i != end; ++i) {
    if (!count--)
      return got;
    out[got++] = i->first.c_str();
  }
  return got;
}

pkgdepdb_bool pkgdepdb_pkg_info_contains_key(pkgdepdb_pkg *pkg_,
                                             const char *key)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  return pkg->info_.find(key) != pkg->info_.end() ? 1 : 0;
}

size_t pkgdepdb_pkg_info_count_values(pkgdepdb_pkg *pkg_, const char *key) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  auto info = pkg->info_.find(key);
  return (info != pkg->info_.end()) ? info->second.size() : 0;
}

size_t pkgdepdb_pkg_info_get_values(pkgdepdb_pkg *pkg_, const char *key,
                                    const char **out, size_t off, size_t count)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  auto info = pkg->info_.find(key);
  if (info == pkg->info_.end())
    return 0;
  return pkgdepdb_strlist_get(info->second, out, off, count);
}

size_t pkgdepdb_pkg_info_add(pkgdepdb_pkg *pkg_, const char *k, const char *v)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  pkg->info_[k].emplace_back(v);
  return 1;
}

size_t pkgdepdb_pkg_info_insert(pkgdepdb_pkg *pkg_, const char *k,
                                size_t index, const char *v)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  auto& key = pkg->info_[k];
  key.insert(key.begin() + index, v);
  return 1;
}

size_t pkgdepdb_pkg_info_insert_r(pkgdepdb_pkg *pkg_, const char *k,
                                  size_t index, size_t count, const char **v)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  auto& key = pkg->info_[k];
  key.insert(key.begin() + index, v, v + count);
  return 1;
}

size_t pkgdepdb_pkg_info_del_s(pkgdepdb_pkg *pkg_, const char *k,
                               const char *v)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  auto info = pkg->info_.find(k);
  if (info == pkg->info_.end())
    return 0;
  auto count = pkgdepdb_strlist_del_s_all(info->second, v);
  if (info->second.empty())
    pkg->info_.erase(info);
  return count;
}

size_t pkgdepdb_pkg_info_del_i(pkgdepdb_pkg *pkg_, const char *k, size_t idx) {
  auto pkg = reinterpret_cast<Package*>(pkg_);
  auto info = pkg->info_.find(k);
  if (info == pkg->info_.end())
    return 0;
  auto &lst = info->second;
  if (idx >= lst.size())
    return 0;
  lst.erase(lst.begin() + idx);
  if (lst.empty())
    pkg->info_.erase(info);
  return 1;
}

size_t pkgdepdb_pkg_info_del_r(pkgdepdb_pkg *pkg_, const char *k, size_t idx,
                               size_t count)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  auto info = pkg->info_.find(k);
  if (info == pkg->info_.end())
    return 0;
  auto &lst = info->second;
  return pkgdepdb_strlist_del_r(lst, idx, count);
}

pkgdepdb_bool pkgdepdb_pkg_info_set_i(pkgdepdb_pkg *pkg_, const char *k,
                                      size_t index, const char *v)
{
  auto pkg = reinterpret_cast<Package*>(pkg_);
  auto info = pkg->info_.find(k);
  if (info == pkg->info_.end())
    return 0;
  auto &lst = info->second;
  return pkgdepdb_strlist_set_i(lst, index, v);
}


} // extern "C"
