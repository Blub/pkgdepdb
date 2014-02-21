#ifndef PKGDEPDB_UTIL_H__
#define PKGDEPDB_UTIL_H__

#include <string>

using std::unique_ptr;
using std::move;
using std::vector;

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
	T *ptr;

	rptr()     : ptr(NULL) {}
	rptr(T *t) : ptr(t) {
		if (ptr) ptr->refcount++;
	}
	rptr(const rptr<T> &o) : ptr(o.ptr) {
		if (ptr) ptr->refcount++;
	}
	rptr(rptr<T> &&o) : ptr(o.ptr) {
		o.ptr = 0;
	}
	~rptr() {
		if (ptr && !--(ptr->refcount))
			delete ptr;
	}
	operator T*() const { return  ptr; }
	T*      get() const { return  ptr; }

	bool operator!() const { return !ptr; }

	T&       operator*()        { return *ptr; }
	T*       operator->()       { return  ptr; }
	const T* operator->() const { return ptr; }

	rptr<T>& operator=(T* o) {
		if (o) o->refcount++;
		if (ptr && !--(ptr->refcount))
			delete ptr;
		ptr = o;
		return (*this);
	}
	rptr<T>& operator=(const rptr<T> &o) {
		if (ptr && !--(ptr->refcount))
			delete ptr;
		if ( (ptr = o.ptr) )
			ptr->refcount++;
		return (*this);
	}
	rptr<T>& operator=(rptr<T> &&o) {
		if (ptr && !--(ptr->refcount))
			delete ptr;
		ptr = o.ptr;
		o.ptr = 0;
		return (*this);
	}
	bool operator==(const T* t) const {
		return ptr == t;
	}
	bool operator==(T* t) const {
		return ptr == t;
	}
	inline T* release() {
		if (!ptr)
			return nullptr;
		auto p = ptr;
		ptr->refcount--;
		ptr = nullptr;
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
inline unique_ptr<T> mk_unique(Args&&... args) {
	return move(unique_ptr<T>(new T(std::forward<Args>(args)...)));
}

template<class T, class... Args>
inline rptr<T> mk_rptr(Args&&... args) {
  return move(rptr<T>(new T(std::forward<Args>(args)...)));
}

// pointer containing a user-defined destructor
// this also contains a refcount so it can be used inside an rptr<>
// note that the refcount doesn't count references to the contained
// pointer, but to the dtor_ptr object itself!
template<typename T>
class dtor_ptr {
public:
	T                      *ptr;
	size_t                  refcount; // to *this* not to ptr
	std::function<void(T*)> dtor_fun;

	dtor_ptr(T *t, std::function<void(T*)> &&fun)
	: ptr(t), refcount(0), dtor_fun(move(fun)) {}
	dtor_ptr(dtor_ptr<T> &&old)
	: ptr(old.ptr), refcount(0), dtor_fun(move(old.dtor_fun) ) { old.ptr = nullptr; }
	dtor_ptr(const dtor_ptr<T> &) = delete;
	dtor_ptr(std::nullptr_t)
	: ptr(nullptr), refcount(0) {}
	~dtor_ptr() {
		if (ptr)
			dtor_fun(ptr);
	}

	operator T*() const { return  ptr; }
	T*      get() const { return  ptr; }
	bool     operator!() const  { return !ptr; }
	T&       operator*()        { return *ptr; }
	T*       operator->()       { return  ptr; }
	const T* operator->() const { return ptr; }
};

namespace util {
	template<typename CONT, typename... Args>
	inline bool
	any(const CONT &lst, const Args&... args) {
		if (!lst.size())
			return true;
		for (auto &i : lst) {
			if ((i)(args...))
				return true;
		}
		return false;
	}

	template<typename CONT, typename... Args>
	inline bool
	all(const CONT &lst, const Args&... args) {
		for (auto &i : lst) {
			if (!(*i)(args...))
				return false;
		}
		return true;
	}
}

#endif
