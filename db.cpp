#include <memory>
#include <algorithm>
#include <utility>

#ifdef ENABLE_THREADS
#  include <atomic>
#  include <thread>
#  include <future>
#  include <unistd.h>
#endif

#include "main.h"

using ObjClass = uint32_t;

static inline ObjClass
getObjClass(unsigned char ei_class, unsigned char ei_data, unsigned char ei_osabi) {
	return (ei_data << 16) | (ei_class << 8) | ei_osabi;
}

static inline ObjClass
getObjClass(Elf *elf) {
	return getObjClass(elf->ei_class, elf->ei_data, elf->ei_osabi);
}

DB::DB() {
	loaded_version = DB::CURRENT;
	contains_package_depends = false;
	strict_linking           = false;
}

DB::~DB() {
	for (auto &pkg : packages)
		delete pkg;
}

PackageList::const_iterator
DB::find_pkg_i(const std::string& name) const
{
	return std::find_if(packages.begin(), packages.end(),
		[&name](const Package *pkg) { return pkg->name == name; });
}

Package*
DB::find_pkg(const std::string& name) const
{
	auto pkg = find_pkg_i(name);
	return (pkg != packages.end()) ? *pkg : nullptr;
}

bool
DB::delete_package(const std::string& name)
{
	Package *old; {
		auto pkgiter = find_pkg_i(name);
		if (pkgiter == packages.end())
			return true;

		old = *pkgiter;
		packages.erase(packages.begin() + (pkgiter - packages.begin()));
	}

	for (auto &elfsp : old->objects) {
		Elf *elf = elfsp.get();
		required_found.erase(elf);
		required_missing.erase(elf);
		// remove the object from the list
		auto self = std::find(objects.begin(), objects.end(), elf);
		objects.erase(self);

		// for each object which depends on this object, search for a replacing object
		for (auto &found : required_found) {
			// does this object depend on 'elf'?
			auto ref = std::find(found.second.begin(), found.second.end(), elf);
			if (ref == found.second.end())
				continue;
			// erase
			found.second.erase(ref);
			// search for this dependency anew
			Elf *seeker = found.first;
			Elf *other;
			if (!seeker->owner || !package_library_path.size())
				other = find_for(seeker, elf->basename, nullptr);
			else {
				auto iter = package_library_path.find(seeker->owner->name);
				if (iter == package_library_path.end())
					other = find_for(seeker, elf->basename, nullptr);
				else
					other = find_for(seeker, elf->basename, &iter->second);
			}
			if (!other) {
				// it's missing now
				required_missing[seeker].insert(elf->basename);
			} else {
				// replace it with a new object
				found.second.insert(other);
			}
		}
	}

	delete old;

	std::remove_if(objects.begin(), objects.end(),
		[](rptr<Elf> &obj) { return 1 == obj->refcount; });

	return true;
}

static bool
pathlist_contains(const std::string& list, const std::string& path)
{
	size_t at = 0;
	size_t to = list.find_first_of(':', 0);
	while (to != std::string::npos) {
		if (list.compare(at, to-at, path) == 0)
			return true;
		at = to+1;
		to = list.find_first_of(':', at);
	}
	if (list.compare(at, std::string::npos, path) == 0)
		return true;
	return false;
}

bool
DB::elf_finds(Elf *elf, const std::string& path, const StringList *extrapaths) const
{
	// DT_RPATH first
	if (elf->rpath_set && pathlist_contains(elf->rpath, path))
		return true;

	// LD_LIBRARY_PATH - ignored

	// DT_RUNPATH
	if (elf->runpath_set && pathlist_contains(elf->runpath, path))
		return true;

	// Trusted Paths
	if (path == "/lib" ||
	    path == "/usr/lib")
	{
		return true;
	}

	if (std::find(library_path.begin(), library_path.end(), path) != library_path.end())
		return true;

	if (extrapaths) {
		if (std::find(extrapaths->begin(), extrapaths->end(), path) != extrapaths->end())
			return true;
	}

	return false;
}

bool
DB::install_package(Package* &&pkg)
{
	if (!delete_package(pkg->name))
		return false;

	packages.push_back(pkg);
	if (pkg->depends.size()    ||
	    pkg->optdepends.size() ||
	    pkg->replaces.size()   ||
	    pkg->conflicts.size()  ||
	    pkg->provides.size())
		contains_package_depends = true;

	const StringList *libpaths = 0;
	if (package_library_path.size()) {
		auto iter = package_library_path.find(pkg->name);
		if (iter != package_library_path.end())
			libpaths = &iter->second;
	}

	for (auto &obj : pkg->objects)
		objects.push_back(obj);

	for (auto &obj : pkg->objects) {
		ObjClass objclass = getObjClass(obj);
		// check if the object is required
		for (auto missing = required_missing.begin(); missing != required_missing.end();) {
			Elf *seeker = missing->first;
			if (getObjClass(seeker) != objclass ||
			    !elf_finds(seeker, obj->dirname, libpaths))
			{
				++missing;
				continue;
			}

			bool needs = (0 != missing->second.erase(obj->basename));

			if (needs) // the library is indeed required and found:
				required_found[seeker].insert(obj);

			if (0 == missing->second.size())
				required_missing.erase(missing++);
			else
				++missing;
		}

		// search for whatever THIS object requires
		link_object_do(obj, pkg);
	}

	return true;
}

Elf*
DB::find_for(Elf *obj, const std::string& needed, const StringList *extrapath) const
{
	log(Debug, "dependency of %s/%s   :  %s\n", obj->dirname.c_str(), obj->basename.c_str(), needed.c_str());
	ObjClass objclass = getObjClass(obj);
	for (auto &lib : objects) {
		if (getObjClass(lib) != objclass) {
			log(Debug, "  skipping %s/%s (objclass)\n", lib->dirname.c_str(), lib->basename.c_str());
			continue;
		}
		if (lib->basename    != needed) {
			log(Debug, "  skipping %s/%s (wrong name)\n", lib->dirname.c_str(), lib->basename.c_str());
			continue;
		}
		if (!elf_finds(obj, lib->dirname, extrapath)) {
			log(Debug, "  skipping %s/%s (not visible)\n", lib->dirname.c_str(), lib->basename.c_str());
			continue;
		}
		// same class, same name, and visible...
		return lib;
	}
	return 0;
}

void
DB::link_object_do(Elf *obj, Package *owner)
{
	ObjectSet req_found;
	StringSet req_missing;
	link_object(obj, owner, req_found, req_missing);
	if (req_found.size())
		required_found[obj] = std::move(req_found);
	if (req_missing.size())
		required_missing[obj] = std::move(req_missing);
}

void
DB::link_object(Elf *obj, Package *owner, ObjectSet &req_found, StringSet &req_missing)
{
	if (ignore_file_rules.size()) {
		std::string full = obj->dirname + "/" + obj->basename;
		if (ignore_file_rules.find(full) != ignore_file_rules.end())
			return;
	}
	const StringList *libpaths = 0;
	if (package_library_path.size()) {
		auto iter = package_library_path.find(owner->name);
		if (iter != package_library_path.end())
			libpaths = &iter->second;
	}

	for (auto &needed : obj->needed) {
		Elf *found = find_for(obj, needed, libpaths);
		if (found)
			req_found.insert(found);
		else
			req_missing.insert(needed);
	}
}

#ifdef ENABLE_THREADS
namespace thread {
	static unsigned int ncpus_init() {
		long v = sysconf(_SC_NPROCESSORS_CONF);
		return (v <= 0 ? 1 : v);
	}

	static unsigned int ncpus = ncpus_init();

	template<typename PerThread>
	void
	work(unsigned long  Count,
	     std::function<void(unsigned long at, unsigned long cnt, unsigned long threads)>
	                    StatusPrinter,
	     std::function<void(std::atomic_ulong*,
	                        size_t from,
	                        size_t to,
	                        PerThread&)>
	                    Worker,
	     std::function<void(std::vector<PerThread>&&)>
	                    Merger)
	{
		unsigned long threadcount = thread::ncpus;
		if (opt_max_jobs > 1 && opt_max_jobs < threadcount)
			threadcount = opt_max_jobs;

		unsigned long  obj_per_thread = Count / threadcount;
		if (!opt_quiet)
			StatusPrinter(0, Count, threadcount);

		if (threadcount == 1) {
			for (unsigned long i = 0; i != Count; ++i) {
				PerThread Data;
				Worker(nullptr, i, i+1, Data);
				if (!opt_quiet)
					StatusPrinter(i, Count, 1);
			}
			return;
		}

		// data created by threads, to be merged in the merger
		std::vector<PerThread> Data;
		Data.resize(threadcount);

		std::atomic_ulong         counter(0);
		std::vector<std::thread*> threads;

		unsigned long i;
		for (i = 0; i != threadcount-1; ++i) {
			threads.emplace_back(
				new std::thread(Worker,
				                &counter,
				                i*obj_per_thread,
				                i*obj_per_thread + obj_per_thread,
				                std::ref(Data[i])));
		}
		threads.emplace_back(
			new std::thread(Worker,
			                &counter,
			                i*obj_per_thread, Count,
			                std::ref(Data[i])));
		if (!opt_quiet) {
			unsigned long c = 0;
			while (c != Count) {
				c = counter.load();
				StatusPrinter(c, Count, threadcount);
				usleep(100000);
			}
		}

		for (i = 0; i != threadcount; ++i) {
			threads[i]->join();
			delete threads[i];
		}
		Merger(std::move(Data));
		if (!opt_quiet)
			StatusPrinter(Count, Count, threadcount);
	}
}

void
DB::relink_all_threaded()
{
	using FoundMap   = std::map<Elf*, ObjectSet>;
	using MissingMap = std::map<Elf*, StringSet>;
	using Tuple      = std::tuple<FoundMap, MissingMap>;
	auto worker = [this](std::atomic_ulong *count, size_t from, size_t to, Tuple &tup) {
		FoundMap   *f = &std::get<0>(tup);
		MissingMap *m = &std::get<1>(tup);
		for (size_t i = from; i != to; ++i) {
			Package *pkg = this->packages[i];

			for (auto &obj : pkg->objects) {
				ObjectSet req_found;
				StringSet req_missing;
				this->link_object(obj, pkg, req_found, req_missing);
				if (req_found.size())
					(*f)[obj] = std::move(req_found);
				if (req_missing.size())
					(*m)[obj] = std::move(req_missing);
			}

			if (count && !opt_quiet)
				(*count)++;
		}
	};
	auto merger = [this](std::vector<Tuple> &&tup) {
		for (auto &t : tup) {
			FoundMap   &found = std::get<0>(t);
			MissingMap &missing = std::get<1>(t);
			for (auto &f : found)
				required_found[f.first] = std::move(f.second);
			for (auto &m : missing)
				required_missing[m.first] = std::move(m.second);
		}
	};
	double fac = 100.0 / double(packages.size());
	unsigned int pc = 1000;
	auto status = [fac, &pc](unsigned long at, unsigned long cnt, unsigned long threadcount) {
		unsigned int newpc = fac * double(at);
		if (newpc == pc)
			return;
		pc = newpc;
		printf("\rrelinking: %3u%% (%lu / %lu packages) [%lu]",
		       pc, at, cnt, threadcount);
		fflush(stdout);
		if (at == cnt)
			printf("\n");
	};
	thread::work<Tuple>(packages.size(), status, worker, merger);
}
#endif

void
DB::relink_all()
{
	required_found.clear();
	required_missing.clear();
	if (!packages.size())
		return;

#ifdef ENABLE_THREADS
	if (opt_max_jobs    != 1   &&
	    thread::ncpus   >  1   &&
	    packages.size() >  100 &&
	    objects.size()  >= 300)
	{
		return relink_all_threaded();
	}
#endif

	unsigned long pkgcount = packages.size();
	double        fac   = 100.0 / double(pkgcount);
	unsigned long count = 0;
	unsigned int  pc    = 0;
	if (!opt_quiet) {
		printf("relinking: 0%% (0 / %lu packages)", pkgcount);
		fflush(stdout);
	}
	for (auto &pkg : packages) {
		for (auto &obj : pkg->objects) {
			link_object_do(obj, pkg);
		}
		if (!opt_quiet) {
			++count;
			unsigned int newpc = fac * double(count);
			if (newpc != pc) {
				pc = newpc;
				printf("\rrelinking: %3u%% (%lu / %lu packages)",
				       pc, count, pkgcount);
				fflush(stdout);
			}
		}
	}
	if (!opt_quiet) {
		printf("\rrelinking: 100%% (%lu / %lu packages)\n",
		       count, pkgcount);
	}
}

void
DB::fix_paths()
{
	for (auto &obj : objects) {
		fixpathlist(obj->rpath);
		fixpathlist(obj->runpath);
	}
}

bool
DB::empty() const
{
	return packages.size()         == 0 &&
	       objects.size()          == 0 &&
	       required_found.size()   == 0 &&
	       required_missing.size() == 0;
}

bool
DB::ld_clear()
{
	if (library_path.size()) {
		library_path.clear();
		return true;
	}
	return false;
}

static std::string
fixcpath(const std::string& dir)
{
	std::string s(dir);
	fixpath(s);
	return std::move(dir);
}

bool
DB::ld_append(const std::string& dir)
{
	return ld_insert(fixcpath(dir), library_path.size());
}

bool
DB::ld_prepend(const std::string& dir)
{
	return ld_insert(fixcpath(dir), 0);
}

bool
DB::ld_delete(size_t i)
{
	if (!library_path.size() || i >= library_path.size())
		return false;
	library_path.erase(library_path.begin() + i);
	return true;
}

bool
DB::ld_delete(const std::string& dir_)
{
	if (!dir_.length())
		return false;
	if (dir_[0] >= '0' && dir_[0] <= '9') {
		return ld_delete(strtoul(dir_.c_str(), nullptr, 0));
	}
	std::string dir(dir_);
	fixpath(dir);
	auto old = std::find(library_path.begin(), library_path.end(), dir);
	if (old != library_path.end()) {
		library_path.erase(old);
		return true;
	}
	return false;
}

bool
DB::ld_insert(const std::string& dir_, size_t i)
{
	std::string dir(dir_);
	fixpath(dir);
	if (i > library_path.size())
		i = library_path.size();

	auto old = std::find(library_path.begin(), library_path.end(), dir);
	if (old == library_path.end()) {
		library_path.insert(library_path.begin() + i, dir);
		return true;
	}
	size_t oldidx = old - library_path.begin();
	if (oldidx == i)
		return false;
	// exists
	library_path.erase(old);
	library_path.insert(library_path.begin() + i, dir);
	return true;
}

bool
DB::pkg_ld_insert(const std::string& package, const std::string& dir_, size_t i)
{
	std::string dir(dir_);
	fixpath(dir);
	StringList &path(package_library_path[package]);

	if (i > path.size())
		i = path.size();

	auto old = std::find(path.begin(), path.end(), dir);
	if (old == path.end()) {
		path.insert(path.begin() + i, dir);
		return true;
	}
	size_t oldidx = old - path.begin();
	if (oldidx == i)
		return false;
	// exists
	path.erase(old);
	path.insert(path.begin() + i, dir);
	return true;
}

bool
DB::pkg_ld_delete(const std::string& package, const std::string& dir_)
{
	std::string dir(dir_);
	fixpath(dir);
	auto iter = package_library_path.find(package);
	if (iter == package_library_path.end())
		return false;

	StringList &path(iter->second);
	auto old = std::find(path.begin(), path.end(), dir);
	if (old != path.end()) {
		path.erase(old);
		if (!path.size())
			package_library_path.erase(iter);
		return true;
	}
	return false;
}

bool
DB::pkg_ld_delete(const std::string& package, size_t i)
{
	auto iter = package_library_path.find(package);
	if (iter == package_library_path.end())
		return false;

	StringList &path(iter->second);
	if (i >= path.size())
		return false;
	path.erase(path.begin()+i);
	if (!path.size())
		package_library_path.erase(iter);
	return true;
}

bool
DB::pkg_ld_clear(const std::string& package)
{
	auto iter = package_library_path.find(package);
	if (iter == package_library_path.end())
		return false;

	package_library_path.erase(iter);
	return true;
}

bool
DB::ignore_file(const std::string& filename)
{
	return std::get<1>(ignore_file_rules.insert(fixcpath(filename)));
}

bool
DB::unignore_file(const std::string& filename)
{
	return (ignore_file_rules.erase(fixcpath(filename)) > 0);
}

bool
DB::unignore_file(size_t id)
{
	if (id >= ignore_file_rules.size())
		return false;
	auto iter = ignore_file_rules.begin();
	while (id) {
		++iter;
		--id;
	}
	ignore_file_rules.erase(iter);
	return true;
}

bool
DB::add_base_package(const std::string& name)
{
	return std::get<1>(base_packages.insert(name));
}

bool
DB::remove_base_package(const std::string& name)
{
	return (base_packages.erase(name) > 0);
}

bool
DB::remove_base_package(size_t id)
{
	if (id >= base_packages.size())
		return false;
	auto iter = base_packages.begin();
	while (id) {
		++iter;
		--id;
	}
	base_packages.erase(iter);
	return true;
}

void
DB::show_info()
{
	if (opt_json & JSONBits::Query)
		return show_info_json();

	printf("DB version: %u\n", loaded_version);
	printf("DB name:    [%s]\n", name.c_str());
	printf("DB flags:   { %s }\n",
	       (strict_linking ? "strict" : "non_strict"));
	printf("Additional Library Paths:\n");
	unsigned id = 0;
	for (auto &p : library_path)
		printf("  %u: %s\n", id++, p.c_str());
	if (ignore_file_rules.size()) {
		printf("Ignoring the following files:\n");
		id = 0;
		for (auto &ign : ignore_file_rules)
			printf("  %u: %s\n", id++, ign.c_str());
	}
	if (package_library_path.size()) {
		printf("Package-specific library paths:\n");
		id = 0;
		for (auto &iter : package_library_path) {
			printf("  %s:\n", iter.first.c_str());
			id = 0;
			for (auto &path : iter.second)
				printf("    %u: %s\n", id++, path.c_str());
		}
	}
	if (base_packages.size()) {
		printf("The following packages are base packages:\n");
		id = 0;
		for (auto &p : base_packages)
			printf("  %u: %s\n", id++, p.c_str());
	}
}

bool
DB::is_broken(const Elf *obj) const
{
	return required_missing.find(const_cast<Elf*>(obj)) != required_missing.end();
}

bool
DB::is_broken(const Package *pkg) const
{
	for (auto &obj : pkg->objects) {
		if (is_broken(obj))
			return true;
	}
	return false;
}

void
DB::show_packages(bool filter_broken)
{
	if (opt_json & JSONBits::Query)
		return show_packages_json(filter_broken);

	if (!opt_quiet)
		printf("Packages:%s\n", (filter_broken ? " (filter: 'broken')" : ""));
	for (auto &pkg : packages) {
		if (filter_broken && !is_broken(pkg))
			continue;
		if (opt_quiet)
			printf("%s\n", pkg->name.c_str());
		else
			printf("  -> %s - %s\n", pkg->name.c_str(), pkg->version.c_str());
		if (opt_verbosity >= 1) {
			for (auto &dep : pkg->depends)
				printf("    depends on: %s\n", dep.c_str());
			for (auto &dep : pkg->optdepends)
				printf("    depends optionally on: %s\n", dep.c_str());
			if (filter_broken) {
				for (auto &obj : pkg->objects) {
					if (is_broken(obj)) {
						printf("    broken: %s / %s\n", obj->dirname.c_str(), obj->basename.c_str());
						if (opt_verbosity >= 2) {
							auto list = required_missing.find(obj);
							for (auto &missing : list->second)
								printf("      misses: %s\n", missing.c_str());
						}
					}
				}
			}
			else {
				for (auto &obj : pkg->objects)
					printf("    contains %s / %s\n", obj->dirname.c_str(), obj->basename.c_str());
			}
		}
	}
}

void
DB::show_objects()
{
	if (opt_json & JSONBits::Query)
		return show_objects_json();

	if (!objects.size()) {
		if (!opt_quiet)
			printf("Objects: none\n");
		return;
	}
	if (!opt_quiet)
		printf("Objects:\n");
	for (auto &obj : objects) {
		if (opt_quiet)
			printf("%s/%s\n", obj->dirname.c_str(), obj->basename.c_str());
		else
			printf("  -> %s / %s\n", obj->dirname.c_str(), obj->basename.c_str());
		if (opt_verbosity < 1)
			continue;
		printf("     class: %u (%s)\n", (unsigned)obj->ei_class, obj->classString());
		printf("     data:  %u (%s)\n", (unsigned)obj->ei_data,  obj->dataString());
		printf("     osabi: %u (%s)\n", (unsigned)obj->ei_osabi, obj->osabiString());
		if (obj->rpath_set)
			printf("     rpath: %s\n", obj->rpath.c_str());
		if (obj->runpath_set)
			printf("     runpath: %s\n", obj->runpath.c_str());
		if (opt_verbosity < 2)
			continue;
		printf("     finds:\n"); {
			auto &set = required_found[obj];
			for (auto &found : set)
				printf("       -> %s / %s\n", found->dirname.c_str(), found->basename.c_str());
		}
		printf("     misses:\n"); {
			auto &set = required_missing[obj];
			for (auto &miss : set)
				printf("       -> %s\n", miss.c_str());
		}
	}
	printf("\n`found` entry set size: %lu\n",
	       (unsigned long)required_found.size());
	printf("`missing` entry set size: %lu\n",
	       (unsigned long)required_missing.size());
}

void
DB::show_missing()
{
	if (opt_json & JSONBits::Query)
		return show_missing_json();

	if (!required_missing.size()) {
		if (!opt_quiet)
			printf("Missing: nothing\n");
		return;
	}
	if (!opt_quiet)
		printf("Missing:\n");
	for (auto &mis : required_missing) {
		Elf       *obj = mis.first;
		StringSet &set = mis.second;
		if (opt_quiet)
			printf("%s/%s\n", obj->dirname.c_str(), obj->basename.c_str());
		else
			printf("  -> %s / %s\n", obj->dirname.c_str(), obj->basename.c_str());
		for (auto &s : set)
			printf("    misses: %s\n", s.c_str());
	}
}

void
DB::show_found()
{
	if (opt_json & JSONBits::Query)
		return show_found_json();

	if (!opt_quiet)
		printf("Found:\n");
	for (auto &fnd : required_found) {
		Elf       *obj = fnd.first;
		ObjectSet &set = fnd.second;
		if (opt_quiet)
			printf("%s/%s\n", obj->dirname.c_str(), obj->basename.c_str());
		else
			printf("  -> %s / %s\n", obj->dirname.c_str(), obj->basename.c_str());
		for (auto &s : set)
			printf("    finds: %s\n", s->basename.c_str());
	}
}

static const Package*
find_depend(std::string dep, const std::map<std::string, const Package*> &pkgmap)
{
	if (!dep.length())
		return 0;

	size_t len = dep.length();
	for (size_t i = 0; i != len; ++i) {
		if (dep[i] == '=' ||
		    dep[i] == '<' ||
		    dep[i] == '>' ||
		    dep[i] == '!')
		{
			dep.erase(i);
			break;
		}
	}

	auto find = pkgmap.find(dep);
	if (find == pkgmap.end())
		return 0;
	return find->second;
}

static void
install_recursive(std::vector<const Package*> &packages,
                  StringSet                   &names,
                  const Package              *pkg,
                  const std::map<std::string, const Package*> &pkgmap)
{
	if (names.find(pkg->name) != names.end())
		return;
	names.insert(pkg->name);
	packages.push_back(pkg);
	for (auto &dep : pkg->depends) {
		auto found = find_depend(dep, pkgmap);
		if (!found) {
			//printf("\rmissing package: %s     \n", dep.c_str());
			continue;
		}
		install_recursive(packages, names, found, pkgmap);
	}
}

void
DB::check_integrity(const Package *pkg,
                    const std::map<std::string, const Package*> &pkgmap,
                    const std::map<std::string, std::vector<const Elf*>> &objmap,
                    const std::vector<const Package*> &package_base) const
{
	std::vector<const Package*> pulled(package_base);
	StringSet                   pulled_names(base_packages);
	install_recursive(pulled, pulled_names, pkg, pkgmap);
	(void)objmap;
}

void
DB::check_integrity() const
{
	log(Message, "Looking for stale object files...\n");
	for (auto &o : objects) {
		if (!o->owner) {
			log(Message, "  object `%s/%s' has no owning package!\n",
			    o->dirname.c_str(), o->basename.c_str());
		}
	}

	log(Message, "Preparing data to check package dependencies...\n");
	// we assume here that std::map has better search performance than O(n)
	std::map<std::string, const Package*>          pkgmap;
	std::map<std::string, std::vector<const Elf*>> objmap;

	for (auto &p: packages)
		pkgmap[p->name] = p;
	for (auto &o: objects) {
		if (o->owner)
			objmap[o->basename].push_back(o);
	}

	// install base system:
	std::vector<const Package*> base;

	for (auto &basepkg : base_packages) {
		auto p = pkgmap.find(basepkg);
		if (p != pkgmap.end())
			base.push_back(p->second);
	}

	double fac = 100.0 / double(packages.size());
	unsigned int pc = 1000;
	auto status = [fac,&pc](unsigned long at, unsigned long cnt, unsigned long threads) {
		unsigned int newpc = fac * double(at);
		if (newpc == pc)
			return;
		pc = newpc;
		printf("\rpackages: %3u%% (%lu / %lu) [%lu]",
		       pc, at, cnt, threads);
		fflush(stdout);
		if (at == cnt)
			printf("\n");
	};

	log(Message, "Checking package dependencies...\n");
#ifndef ENABLE_THREADS
	status(0, packages.size(), 1);
	for (size_t i = 0; i != packages.size(); ++i) {
		check_integrity(packages[i], pkgmap, objmap, base);
		status(i, packages.size(), 1);
	}
#else
	auto merger = [](std::vector<int> &&n) {
		(void)n;
	};
	auto worker = [this,&pkgmap,&objmap,&base](std::atomic_ulong *count, size_t from, size_t to, int &dummy) {
		(void)dummy;

		for (size_t i = from; i != to; ++i) {
			const Package *pkg = packages[i];
			check_integrity(pkg, pkgmap, objmap, base);
			if (count)
				++*count;
		}
	};
	thread::work<int>(packages.size(), status, worker, merger);
#endif
}
