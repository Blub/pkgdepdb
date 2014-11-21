#ifndef PKGDEPDB_CAPI_ALGORITHM_H__
#define PKGDEPDB_CAPI_ALGORITHM_H__

#include <set>
#include <string>
#include <vector>
#include <algorithm>

template<class T, class STRLIST>
size_t pkgdepdb_strlist_count(const T& obj, STRLIST T::*member) {
  return (obj.*member).size();
}

template<class STRLIST>
size_t pkgdepdb_strlist_get(const STRLIST& lst, const char **out, size_t off,
                            size_t count)
{
  auto size = lst.size();
  if (off >= size)
    return 0;

  auto i    = lst.begin();
  auto end  = lst.end();

  // in case of std::set:
  while (off--)
    ++i;

  size_t got = 0;
  for (; i != end; ++i) {
    if (!count--)
      return got;
    out[got++] = i->c_str();
  }
  return got;
}

template<class T, class STRLIST>
size_t pkgdepdb_strlist_get(const T& obj, STRLIST T::*member,
                            const char **out, size_t off, size_t count)
{
  return pkgdepdb_strlist_get(obj.*member, out, off, count);
}

template<class T>
size_t pkgdepdb_strlist_add_unique(T& obj, std::set<std::string> T::*member,
                                   const char *value)
{
  return std::get<1>((obj.*member).insert(value)) ? 1 : 0;
}

template<class T>
size_t pkgdepdb_strlist_insert_unique(T& obj,
                                      std::set<std::string> T::*member,
                                      size_t count, const char **values)
{
  auto& v = obj.*member;
  v.insert(values, values + count);
  return 1;
}

template<class T>
size_t pkgdepdb_strlist_add_always(T& obj, std::vector<std::string> T::*member,
                                   const char *value)
{
  (obj.*member).emplace_back(value);
  return 1;
}

template<class T>
size_t pkgdepdb_strlist_add_unique(T& obj, std::vector<std::string> T::*member,
                                   const char *value)
{
  auto& v = obj.*member;
  auto beg = v.begin();
  auto end = v.end();
  if (std::find(beg, end, value) == end)
    return 0;
  v.emplace_back(value);
  return 1;
}

template<class T>
size_t pkgdepdb_strlist_insert_always(T& obj,
                                      std::vector<std::string> T::*member,
                                      size_t index, size_t count,
                                      const char **values)
{
  auto& v = obj.*member;
  v.insert(v.begin() + index, values, values + count);
  return 1;
}

template<class T>
size_t pkgdepdb_strlist_insert_unique(T& obj,
                                      std::vector<std::string> T::*member,
                                      size_t index, size_t count,
                                      const char **values)
{
  auto& mem = obj.*member;
  std::vector<std::string> vec(values, values + count);
  for (const auto& v : mem) {
    auto found = std::find(vec.begin(), vec.end(), v);
    if (found != vec.end())
      vec.erase(found);
  }
  mem.insert(mem.begin() + index,
             std::make_move_iterator(vec.begin()),
             std::make_move_iterator(vec.end()));
  return vec.size();
}

static inline
size_t pkgdepdb_strlist_del_s_one(std::vector<std::string>& lst, const char *v)
{
  auto beg = lst.begin();
  auto end = lst.end();
  auto it  = std::find(beg, end, v);
  if (it == end)
    return 0;
  lst.erase(it);
  return 1;
}

static inline
size_t pkgdepdb_strlist_del_s_one(std::set<std::string>& lst, const char *v) {
  return lst.erase(v) ? 1 : 0;
}

template<class T>
size_t pkgdepdb_strlist_del_s_one(T& obj, std::set<std::string> T::*member,
                                  const char *value)
{
  return (obj.*member).erase(value) ? 1 : 0;
}

template<class T>
size_t pkgdepdb_strlist_del_s_one(T& obj, std::vector<std::string> T::*member,
                                  const char *value)
{
  return pkgdepdb_strlist_del_s_one(obj.*member, value);
}

static inline
size_t pkgdepdb_strlist_del_s_all(std::set<std::string>& lst, const char *v) {
  return lst.erase(v) ? 1 : 0;
}

template<class T>
size_t pkgdepdb_strlist_del_s_all(T& obj, std::set<std::string> T::*member,
                                  const char *value)
{
  return (obj.*member).erase(value) ? 1 : 0;
}

static inline
size_t pkgdepdb_strlist_del_s_all(std::vector<std::string>& lst, const char *v)
{
  auto beg = lst.begin();
  auto end = lst.end();
  auto it  = std::remove(beg, end, v);
  end = lst.end(); // std::remove is a write operation after all
  size_t count = end - it;
  lst.erase(it, end);
  return count;
}

template<class T>
size_t pkgdepdb_strlist_del_s_all(T& obj, std::vector<std::string> T::*member,
                                  const char *value)
{
  return pkgdepdb_strlist_del_s_all(obj.*member, value);
}

template<class T, class STRLIST>
size_t pkgdepdb_strlist_del_i(T& obj, STRLIST T::*member, size_t index) {
  if (index >= (obj.*member).size())
    return 0;
  auto i = (obj.*member).begin();
  while (index--)
    ++i;
  (obj.*member).erase(i);
  return 1;
}

template<class STRLIST>
size_t pkgdepdb_strlist_del_r(STRLIST& lst, size_t index, size_t count) {
  size_t max = lst.size();
  if (index >= max)
    return 0;
  if (count > max - index)
    count = max - index;
  auto i = lst.begin();
  while (index--) ++i;
  auto e = i;
  for (index = count; index--;) ++e;
  lst.erase(i, e);
  return count;
}

static inline
bool pkgdepdb_strlist_set_i(std::vector<std::string>& lst, size_t index,
                            const char *value)
{
  if (index >= lst.size())
    return false;
  lst[index] = value;
  return true;
}

template<class STRLIST>
bool pkgdepdb_strlist_contains(const STRLIST& lst, const char *value) {
  return std::find(lst.begin(), lst.end(), value) != lst.end();
}

#endif
