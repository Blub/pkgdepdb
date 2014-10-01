#ifndef PKGDEPDB_UTIL_H__
#define PKGDEPDB_UTIL_H__

namespace pkgdepdb {

class strref {
public:
  static std::string empty;
  const std::string &s_;
  strref() : s_(empty) {}
  strref(const std::string &s) : s_(s) {}
  inline operator const std::string&() const;
  inline bool operator< (const std::string &s) const;
  inline bool operator<=(const std::string &s) const;
  inline bool operator==(const std::string &s) const;
  inline bool operator!=(const std::string &s) const;
  inline bool operator>=(const std::string &s) const;
  inline bool operator> (const std::string &s) const;
  inline const std::string& operator*() const;
  inline const std::string* operator->() const;
};

strref::operator const std::string&() const { return s_; }
bool strref::operator< (const std::string &s) const { return s_ <  s; }
bool strref::operator<=(const std::string &s) const { return s_ <= s; }
bool strref::operator==(const std::string &s) const { return s_ == s; }
bool strref::operator!=(const std::string &s) const { return s_ != s; }
bool strref::operator>=(const std::string &s) const { return s_ >= s; }
bool strref::operator> (const std::string &s) const { return s_ >  s; }
const std::string& strref::operator*() const { return s_; }
const std::string* strref::operator->() const { return &s_; }

template<typename T>
class rptr {
public:
  T *ptr_;

  constexpr rptr() : ptr_(nullptr) {}
  rptr(T *t) : ptr_(t) {
    if (ptr_) ptr_->refcount_++;
  }
  rptr(const rptr<T> &o) : ptr_(o.ptr_) {
    if (ptr_) ptr_->refcount_++;
  }
  rptr(rptr<T> &&o) : ptr_(o.ptr_) {
    o.ptr_ = 0;
  }
  ~rptr() {
    if (ptr_ && !--(ptr_->refcount_))
      delete ptr_;
  }
  operator T*() const { return  ptr_; }
  T*      get() const { return  ptr_; }

  bool operator!() const { return !ptr_; }

  T&       operator*()        { return *ptr_; }
  T*       operator->()       { return  ptr_; }
  const T* operator->() const { return  ptr_; }

  rptr<T>& operator=(T* o) {
    if (o) o->refcount_++;
    if (ptr_ && !--(ptr_->refcount_))
      delete ptr_;
    ptr_ = o;
    return (*this);
  }
  rptr<T>& operator=(const rptr<T> &o) {
    if (o.ptr_) o.ptr_->refcount_++;
    if (ptr_ && !--(ptr_->refcount_))
      delete ptr_;
    ptr_ = o.ptr_;
    return (*this);
  }
  rptr<T>& operator=(rptr<T> &&o) {
    if (this == &o)
      return (*this);
    if (ptr_ && !--(ptr_->refcount_))
      delete ptr_;
    ptr_ = o.ptr_;
    o.ptr_ = 0;
    return (*this);
  }
  bool operator==(const T* t) const {
    return ptr_ == t;
  }
  bool operator==(T* t) const {
    return ptr_ == t;
  }
  inline T* release() {
    if (!ptr_)
      return nullptr;
    auto p = ptr_;
    ptr_->refcount_--;
    ptr_ = nullptr;
    return p;
  }
};

class guard {
public:
  bool                  on;
  std::function<void()> fn;
  guard(std::function<void()> f)
  : on(true), fn(f) {}
  ~guard() {
    if (on) fn();
  }
  void release() { on = false; }
};

template<class T, class... Args>
inline uniq<T> mk_unique(Args&&... args) {
  return move(uniq<T>(new T(std::forward<Args>(args)...)));
}

template<class T, class... Args>
inline rptr<T> mk_rptr(Args&&... args) {
  return move(rptr<T>(new T(std::forward<Args>(args)...)));
}

namespace util {

template<typename CONT, typename... Args>
inline bool any(const CONT &lst, const Args&... args) {
  if (!lst.size())
    return true;
  for (auto &i : lst) {
    if ((i)(args...))
      return true;
  }
  return false;
}

template<typename CONT, typename... Args>
inline bool all(const CONT &lst, const Args&... args) {
  for (auto &i : lst) {
    if (!(*i)(args...))
      return false;
  }
  return true;
}

} // ::pkgdepdb::util

} // ::pkgdepdb

#endif
