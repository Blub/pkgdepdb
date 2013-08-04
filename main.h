#ifndef WDEPTRACK_MAIN_H__
#define WDEPTRACK_MAIN_H__

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>

#ifndef PKGDEPDB_V_MAJ
# error "PKGDEPDB_V_MAJ not defined"
#endif
#ifndef PKGDEPDB_V_MIN
# error "PKGDEPDB_V_MIN not defined"
#endif
#ifndef PKGDEPDB_V_PAT
# error "PKGDEPDB_V_PAT not defined"
#endif

#ifndef PKGDEPDB_ETC
# error "No PKGDEPDB_ETC defined"
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

extern std::string  opt_default_db;
extern unsigned int opt_verbosity;
extern bool         opt_quiet;
extern bool         opt_package_depends;
extern unsigned int opt_max_jobs;
extern unsigned int opt_json;

namespace JSONBits {
	enum {
		Query = (1<<0),
		DB    = (1<<1)
	};
}

bool ReadConfig     ();
bool CfgStrToBool   (const std::string& line);
bool CfgParseJSONBit(const char *bit);

class Package;

class Elf {
public:
	Elf();
	Elf(const Elf& cp);
	static Elf* open(const char *data, size_t size, bool *waserror, const char *name);

public:
	size_t refcount;

	// path + name separated
	std::string dirname;
	std::string basename;

	// classification:
	unsigned char ei_class; // 32/64 bit
	unsigned char ei_data;  // endianess
	unsigned char ei_osabi; // freebsd/linux/...

	// requirements:
	bool                     rpath_set;
	bool                     runpath_set;
	std::string              rpath;
	std::string              runpath;
	std::vector<std::string> needed;

public: // utility functions while loading
	void solve_paths(const std::string& origin);
	bool can_use(const Elf &other, bool strict) const;

public: // utility functions for printing stuff
	const char *classString() const;
	const char *dataString()  const;
	const char *osabiString() const;

public: // NOT SERIALIZED:
	struct {
		size_t id;
	} json;

	Package *owner;
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

using PackageList = std::vector<Package*>;
using ObjectList  = std::vector<rptr<Elf>>;
using StringList  = std::vector<std::string>;

using ObjectSet   = std::set<rptr<Elf>>;
using StringSet   = std::set<std::string>;

/// Package class
/// reads a package archive, extracts information
class Package {
public:
	static Package* open(const std::string& path);

	std::string             name;
	std::string             version;
	std::vector<rptr<Elf> > objects;

	// DB version 3:
	StringList              depends;
	StringList              optdepends;
	StringList              provides;
	StringList              conflicts;
	StringList              replaces;

	void show_needed();
	Elf* find(const std::string &dirname, const std::string &basename) const;

public: // loading utiltiy functions
	void guess(const std::string& name);

public: // NOT SERIALIZED:
	// used only while loading an archive
	struct {
		std::map<std::string, std::string> symlinks;
	} load;
};

void fixpath(std::string& path);
void fixpathlist(std::string& pathlist);

using PkgMap     = std::map<std::string, const Package*>;
using PkgListMap = std::map<std::string, std::vector<const Package*>>;
using ObjListMap = std::map<std::string, std::vector<const Elf*>>;

class DB {
public:
	static uint16_t CURRENT;

	using IgnoreMissingRule = struct {
		std::string package;
		std::string object;
		std::string what;
	};

	DB();
	~DB();

	uint16_t    loaded_version;
	bool        strict_linking; // stored as flag bit

	std::string name;
	StringList  library_path;

	PackageList packages;
	ObjectList  objects;

	std::map<Elf*, ObjectSet> required_found;
	std::map<Elf*, StringSet> required_missing;

	StringSet                         ignore_file_rules;
	std::map<std::string, StringList> package_library_path;
	StringSet                         base_packages;

public:
	bool install_package(Package* &&pkg);
	bool delete_package (const std::string& name);
	Elf *find_for       (Elf*, const std::string& lib, const StringList *extrapath) const;
	void link_object    (Elf*, Package *owner, ObjectSet &req_found, StringSet &req_missing);
	void link_object_do (Elf*, Package *owner);
	void relink_all     ();
	void fix_paths      ();
	bool wipe_packages  ();

#ifdef ENABLE_THREADS
	void relink_all_threaded();
#endif

	Package* find_pkg   (const std::string& name) const;
	PackageList::const_iterator
	         find_pkg_i (const std::string& name) const;

	void show_info();
	void show_info_json();
	void show_packages(bool filter_broken);
	void show_packages_json(bool filter_broken);
	void show_objects();
	void show_objects_json();
	void show_missing();
	void show_missing_json();
	void show_found();
	void show_found_json();
	void check_integrity() const;
	void check_integrity(const Package    *pkg,
                         const PkgMap     &pkgmap,
                         const PkgListMap &providemap,
                         const PkgListMap &replacemap,
                         const ObjListMap &objmap,
                         const std::vector<const Package*> &base) const;

	bool store(const std::string& filename);
	bool read (const std::string& filename);
	bool empty() const;

	bool ld_append (const std::string& dir);
	bool ld_prepend(const std::string& dir);
	bool ld_delete (const std::string& dir);
	bool ld_delete (size_t i);
	bool ld_insert (const std::string& dir, size_t i);
	bool ld_clear  ();

	bool ignore_file  (const std::string& name);
	bool unignore_file(const std::string& name);
	bool unignore_file(size_t id);

	bool add_base_package   (const std::string& name);
	bool remove_base_package(const std::string& name);
	bool remove_base_package(size_t id);

	bool pkg_ld_insert(const std::string& package, const std::string& path, size_t i);
	bool pkg_ld_delete(const std::string& package, const std::string& path);
	bool pkg_ld_delete(const std::string& package, size_t i);
	bool pkg_ld_clear (const std::string& package);

private:
	bool elf_finds(Elf*, const std::string& lib, const StringList *extrapath) const;

	bool is_broken(const Package *pkg) const;
	bool is_broken(const Elf *elf) const;

public: // NOT SERIALIZED:
	bool contains_package_depends;
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
