#include <algorithm>
#include <memory>
#include <map>

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

using PackageList = std::vector<Package*>;
using ObjectList  = std::vector<rptr<Elf>>;
using StringList  = std::vector<std::string>;

class DB {
public:
	~DB();

	PackageList packages;
	ObjectList  objects;

	std::map<Elf*, ObjectList> required_found;
	std::map<Elf*, StringList> required_missing;

	bool install_package(Package* &&pkg);
	bool delete_package(const std::string& name);

	Package* find_pkg(const std::string& name) const;
	PackageList::const_iterator find_pkg_i(const std::string& name) const;
};

DB::~DB() {
	for (auto &pkg : packages)
		delete pkg;
	for (auto &obj : objects)
		delete obj;
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
	if (path == "/lib" ||
	    path == "/usr/lib")
	{
		return true;
	}

	if (!elf->rpath.length())
		return false;

	size_t at = 0;
	while (at != std::string::npos) {
		size_t to = elf->rpath.find_first_of(':', at);
		if (elf->rpath.substr(at, to) == path)
			return true;
		at = to;
	}

	return false;
}

bool
DB::install_package(Package* &&pkg)
{
	if (!delete_package(pkg->name))
		return false;
	packages.push_back(pkg);

	for (auto &obj : pkg->objects) {
		ObjClass objclass = getObjClass(obj);
		// check if the object is required
		for (auto missing = required_missing.begin(); missing != required_missing.end();) {
			if (getObjClass(missing->first) != objclass ||
			    !elf_finds(missing->first, obj->dirname))
			{
				++missing;
				continue;
			}
			std::remove_if(missing->second.begin(), missing->second.end(),
				[&obj](const std::string &lib) { return lib == obj->basename; });
			if (0 == missing->second.size())
				required_missing.erase(missing++);
			else
				++missing;
		}
	}

	return true;
}
