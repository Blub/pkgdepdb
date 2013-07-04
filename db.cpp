#include <memory>
#include <algorithm>
#include <utility>

#include "main.h"

using ObjClass = unsigned short;

static inline ObjClass
getObjClass(unsigned char ei_class, unsigned char ei_osabi) {
	return (ei_class << 8) | ei_osabi;
}

static inline ObjClass
getObjClass(Elf *elf) {
	return getObjClass(elf->ei_class, elf->ei_osabi);
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
		{
			auto found = required_found.find(elf);
			if (found != required_found.end())
				required_found.erase(found);
		}{
			auto missing = required_missing.find(elf);
			if (missing != required_missing.end())
				required_missing.erase(missing);
		}
	}

	delete old;

	std::remove_if(objects.begin(), objects.end(),
		[](rptr<Elf> &obj) { return 1 == obj->refcount; });

	return true;
}

static bool
elf_finds(Elf *elf, const std::string& path)
{
	size_t at;

	// DT_RPATH first
	if (elf->rpath_set) {
		at = 0;
		while (at != std::string::npos) {
			size_t to = elf->rpath.find_first_of(':', at);
			std::string p(elf->rpath.substr(at, to));
			if (p.length() && p == path)
				return true;
			at = to;
		}
	}

	// LD_LIBRARY_PATH - ignored

	// DT_RUNPATH
	if (elf->runpath_set) {
		at = 0;
		while (at != std::string::npos) {
			size_t to = elf->runpath.find_first_of(':', at);
			std::string p(elf->runpath.substr(at, to));
			if (p.length() && p == path)
				return true;
			at = to;
		}
	}

	// Trusted Paths
	if (path == "/lib" ||
	    path == "/usr/lib")
	{
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
			/* for vector:
			bool needs = false;
			std::remove_if(missing->second.begin(), missing->second.end(),
				[&obj, &needs](const std::string &lib) {
					// if lib==name {needs=true} written as side-effect
					return (lib == obj->basename && (needs = true));
				});
			*/
			// for std::set:
			bool needs = (0 != missing->second.erase(obj->basename));

			if (needs) // the library is indeed required and found:
				required_found[seeker].insert(obj);

			if (0 == missing->second.size())
				required_missing.erase(missing++);
			else
				++missing;
		}

		// search for whatever THIS object requires
		ObjectSet req_found;
		StringSet req_missing;
		for (auto &needed : obj->needed) {
			Elf *found = find_for(obj, needed);
			if (found)
				req_found.insert(found);
			else
				req_missing.insert(needed);
		}
		required_found[obj]   = std::move(req_found);
		required_missing[obj] = std::move(req_missing);
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
DB::show()
{
	printf("Packages:\n");
	for (auto &pkg : packages)
		printf("  -> %s\n", pkg->name.c_str());
	printf("\nObjects:\n");
	for (auto &obj : objects) {
		printf("  -> %s / %s\n", obj->dirname.c_str(), obj->basename.c_str());
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
		if (obj->rpath_set)
			printf("     rpath: %s\n", obj->rpath.c_str());
		if (obj->runpath_set)
			printf("     runpath: %s\n", obj->runpath.c_str());
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
