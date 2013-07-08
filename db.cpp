#include <memory>
#include <algorithm>
#include <utility>

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
			Elf *other  = find_for(seeker, elf->basename);
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
DB::elf_finds(Elf *elf, const std::string& path) const
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

	return false;
}

bool
DB::install_package(Package* &&pkg)
{
	if (!delete_package(pkg->name))
		return false;

	packages.push_back(pkg);

	for (auto &obj : pkg->objects)
		objects.push_back(obj);

	for (auto &obj : pkg->objects) {
		ObjClass objclass = getObjClass(obj);
		// check if the object is required
		for (auto missing = required_missing.begin(); missing != required_missing.end();) {
			Elf *seeker = missing->first;
			if (getObjClass(seeker) != objclass ||
			    !elf_finds(seeker, obj->dirname))
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
		link_object(obj);
	}

	return true;
}

Elf*
DB::find_for(Elf *obj, const std::string& needed) const
{
	log(Debug, "dependency of %s/%s   :  %s\n", obj->dirname.c_str(), obj->basename.c_str(), needed.c_str());
	ObjClass objclass = getObjClass(obj);
	for (auto &lib : objects) {
		if (getObjClass(lib) != objclass)
			log(Debug, "  skipping %s/%s (objclass)\n", lib->dirname.c_str(), lib->basename.c_str());
		else if (lib->basename    != needed)
			log(Debug, "  skipping %s/%s (wrong name)\n", lib->dirname.c_str(), lib->basename.c_str());
		else if (!elf_finds(obj, lib->dirname))
			log(Debug, "  skipping %s/%s (not visible)\n", lib->dirname.c_str(), lib->basename.c_str());

		if (getObjClass(lib) != objclass ||
		    lib->basename    != needed   ||
		    !elf_finds(obj, lib->dirname))
		{
			continue;
		}
		// same class, same name, and visible...
		return lib;
	}
	return 0;
}

void
DB::link_object(Elf *obj)
{
	ObjectSet req_found;
	StringSet req_missing;
	for (auto &needed : obj->needed) {
		Elf *found = find_for(obj, needed);
		if (found)
			req_found.insert(found);
		else
			req_missing.insert(needed);
	}
	if (req_found.size())
		required_found[obj]   = std::move(req_found);
	if (req_missing.size())
		required_missing[obj] = std::move(req_missing);
}

void
DB::relink_all()
{
	required_found.clear();
	required_missing.clear();
	for (auto &obj : objects) {
		link_object(obj);
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

bool
DB::ld_append(const std::string& dir)
{
	auto old = std::find(library_path.begin(), library_path.end(), dir);
	if (old == library_path.begin() + (library_path.size()-1))
		return false;
	if (old != library_path.end())
		library_path.erase(old);
	library_path.push_back(dir);
	return true;
}

bool
DB::ld_prepend(const std::string& dir)
{
	auto old = std::find(library_path.begin(), library_path.end(), dir);
	if (old == library_path.begin())
		return false;
	if (old != library_path.end())
		library_path.erase(old);
	library_path.insert(library_path.begin(), dir);
	return true;
}

bool
DB::ld_delete(const std::string& dir)
{
	auto old = std::find(library_path.begin(), library_path.end(), dir);
	if (old != library_path.end()) {
		library_path.erase(old);
		return true;
	}
	return false;
}

bool
DB::ld_insert(const std::string& dir, size_t i)
{
	if (!library_path.size())
		i = 0;
	else if (i >= library_path.size())
		i = library_path.size()-1;

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

void
DB::show_info()
{
	printf("DB version: %u\n", (unsigned)DB::version);
	printf("DB name:    [%s]\n", name.c_str());
	printf("Additional Library Paths:\n");
	unsigned id = 0;
	for (auto &p : library_path)
		printf("  %u: %s\n", id++, p.c_str());
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
	if (opt_use_json)
		return show_packages_json(filter_broken);

	printf("Packages:%s\n", (filter_broken ? " (filter: 'broken')" : ""));
	for (auto &pkg : packages) {
		if (filter_broken && !is_broken(pkg))
			continue;
		printf("  -> %s - %s\n", pkg->name.c_str(), pkg->version.c_str());
		if (opt_verbosity >= 1) {
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
	if (opt_use_json)
		return show_objects_json();

	if (!objects.size()) {
		printf("Objects: none\n");
		return;
	}
	printf("Objects:\n");
	for (auto &obj : objects) {
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
	if (opt_use_json)
		return show_missing_json();

	if (!required_missing.size()) {
		printf("Missing: nothing\n");
		return;
	}
	printf("Missing:\n");
	for (auto &mis : required_missing) {
		Elf       *obj = mis.first;
		StringSet &set = mis.second;
		printf("  -> %s / %s\n", obj->dirname.c_str(), obj->basename.c_str());
		for (auto &s : set)
			printf("    misses: %s\n", s.c_str());
	}
}

void
DB::show_found()
{
	if (opt_use_json)
		return show_found_json();

	printf("Found:\n");
	for (auto &fnd : required_found) {
		Elf       *obj = fnd.first;
		ObjectSet &set = fnd.second;
		printf("  -> %s / %s\n", obj->dirname.c_str(), obj->basename.c_str());
		for (auto &s : set)
			printf("    finds: %s\n", s->basename.c_str());
	}
}
