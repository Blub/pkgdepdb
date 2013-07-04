#ifndef WDEPTRACK_MAIN_H__
#define WDEPTRACK_MAIN_H__

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>

#include <elf.h>

#ifndef READPKGELF_V_MAJ
# error "READPKGELF_V_MAJ not defined"
#endif
#ifndef READPKGELF_V_MIN
# error "READPKGELF_V_MIN not defined"
#endif
#ifndef READPKGELF_V_PAT
# error "READPKGELF_V_PAT not defined"
#endif

#define RPKG_IND_STRING2(x) #x
#define RPKG_IND_STRING(x) RPKG_IND_STRING2(x)
#define VERSION_STRING \
	RPKG_IND_STRING(READPKGELF_V_MAJ) "." \
	RPKG_IND_STRING(READPKGELF_V_MIN) "." \
	RPKG_IND_STRING(READPKGELF_V_PAT)

#ifdef GITINFO
# define FULL_VERSION_STRING \
    VERSION_STRING "-git: " GITINFO
#else
# define FULL_VERSION_STRING VERSION_STRING
#endif

enum {
	Debug,
	Warn,
	Error
};

void log(int level, const char *msg, ...);

class Elf {
public:
	Elf();
	static Elf* open(const char *data, size_t size, bool *waserror);

public:
	size_t refcount;

	// path + name separated
	std::string dirname;
	std::string basename;

	// classification:
	unsigned char ei_class; // 32/64 bit
	unsigned char ei_osabi; // freebsd/linux/...

	// requirements:
	bool                     rpath_set;
	bool                     runpath_set;
	std::string              rpath;
	std::string              runpath;
	std::vector<std::string> needed;
};

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
	T* release() {
		T *p = ptr;
		ptr = NULL;
		if (p)
			p->refcount--;
		return p;
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
};

/// Package class
/// reads a package archive, extracts information
class Package {
public:
	static Package* open(const std::string& path);

	std::string                        name;
	std::vector<rptr<Elf> > objects;

	void show_needed();
};

using PackageList = std::vector<Package*>;
using ObjectList  = std::vector<rptr<Elf>>;

using ObjectSet   = std::set<rptr<Elf>>;
using StringSet   = std::set<std::string>;

class DB {
public:
	~DB();

	PackageList packages;
	ObjectList  objects;

	std::map<Elf*, ObjectSet> required_found;
	std::map<Elf*, StringSet> required_missing;

	bool install_package(Package* &&pkg);
	bool delete_package (const std::string& name);
	Elf *find_for       (Elf*, const std::string& lib) const;

	Package* find_pkg   (const std::string& name) const;
	PackageList::const_iterator
	         find_pkg_i (const std::string& name) const;

	void show();

	void store(const std::string& filename);
};

#endif
