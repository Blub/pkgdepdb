#ifndef WDEPTRACK_MAIN_H__
#define WDEPTRACK_MAIN_H__

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>

#include <elf.h>

#ifndef PKGDEPDB_V_MAJ
# error "PKGDEPDB_V_MAJ not defined"
#endif
#ifndef PKGDEPDB_V_MIN
# error "PKGDEPDB_V_MIN not defined"
#endif
#ifndef PKGDEPDB_V_PAT
# error "PKGDEPDB_V_PAT not defined"
#endif

#define RPKG_IND_STRING2(x) #x
#define RPKG_IND_STRING(x) RPKG_IND_STRING2(x)
#define VERSION_STRING \
	RPKG_IND_STRING(PKGDEPDB_V_MAJ) "." \
	RPKG_IND_STRING(PKGDEPDB_V_MIN) "." \
	RPKG_IND_STRING(PKGDEPDB_V_PAT)

#ifdef GITINFO
# define FULL_VERSION_STRING \
    VERSION_STRING "-git: " GITINFO
#else
# define FULL_VERSION_STRING VERSION_STRING
#endif

enum {
	Debug,
	Message, // color ftw
	Print,   // regular printing
	Warn,
	Error
};

void log(int level, const char *msg, ...);

extern unsigned int opt_verbosity;

class Elf {
public:
	Elf();
	Elf(const Elf& cp);
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

public: // utility functions while loading
	void solve_paths(const std::string& origin);

public: // NOT SERIALIZED:
	struct {
		size_t id;
	} json;
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

	std::string             name;
	std::string             version;
	std::vector<rptr<Elf> > objects;

	void show_needed();
	Elf* find(const std::string &dirname, const std::string &basename) const;

public: // NOT SERIALIZED:
	// used only while loading an archive
	struct {
		std::map<std::string, std::string> symlinks;
	} load;
};

using PackageList = std::vector<Package*>;
using ObjectList  = std::vector<rptr<Elf>>;
using StringList  = std::vector<std::string>;

using ObjectSet   = std::set<rptr<Elf>>;
using StringSet   = std::set<std::string>;

class DB {
public:
	static size_t version;

	~DB();

	std::string name;
	StringList  library_path;

	PackageList packages;
	ObjectList  objects;

	std::map<Elf*, ObjectSet> required_found;
	std::map<Elf*, StringSet> required_missing;

public:
	bool install_package(Package* &&pkg);
	bool delete_package (const std::string& name);
	Elf *find_for       (Elf*, const std::string& lib) const;
	void link_object    (Elf*);
	void relink_all     ();

	Package* find_pkg   (const std::string& name) const;
	PackageList::const_iterator
	         find_pkg_i (const std::string& name) const;

	void show_info();
	void show_packages();
	void show_objects();
	void show_missing();
	void show_found();

	bool store(const std::string& filename);
	bool read (const std::string& filename);
	bool empty() const;

	bool ld_append(const std::string& dir);
	bool ld_prepend(const std::string& dir);
	bool ld_delete(const std::string& dir);
	bool ld_insert(const std::string& dir, size_t i);
	bool ld_clear();

private:
	bool elf_finds      (Elf*, const std::string& lib) const;
};

// Utility functions:
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

#endif
