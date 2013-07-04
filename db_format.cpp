#include <stdint.h>

#include <memory>
#include <algorithm>
#include <utility>

#include <fstream>

#include "main.h"

using std::ofstream;

// Simple straight forward data serialization by keeping track
// of already-serialized objects.
// Lame, but effective.

enum class ObjRef : unsigned uint8_t {
	PKG,
	PKGREF,
	OBJ,
	OBJREF
};

using PkgOutMap = std::map<Package*, size_t>;
using ObjOutMap = std::map<Elf*,     size_t>;

static const char
depdb_magic[] = { 'D', 'E', 'P',
                  'D', 'B',
                  0, 0, 1 };

class SerialOut {
public:
	DB        *db;
	ofstream   out;
	PkgOutMap  pkgs;
	ObjOutMap  objs;

	SerialOut(DB *db_, const std::string& filename)
	: db(db_), out(filename.c_str(), std::ios::binary)
	{
		if (out)
			out.write(depdb_magic, sizeof(depdb_magic));
	}
};

// Using the <= operator - we're "streaming" here, so whatever
// C++ stream << is forbidden due to its extreme stupidity

template<typename T>
static inline SerialOut&
operator<=(SerialOut &out, const T& r)
{
	out.out.write((const char*)&r, sizeof(r));
	return out;
}

// special for strings:
static inline SerialOut&
operator<=(SerialOut &out, const std::string& r)
{
	uint32_t len = r.length();
	out.out.write((const char*)&len, sizeof(len));
	out.out.write(r.c_str(), len);
	return out;
}

static bool write_obj(SerialOut &out, Elf *obj);

static bool
write_objlist(SerialOut &out, const ObjectList& list)
{
	uint32_t len = list.size();
	out.out.write((const char*)&len, sizeof(len));
	for (auto &obj : list) {
		if (!write_obj(out, obj))
			return false;
	}
	return true;
}
static bool
write_objset(SerialOut &out, const ObjectSet& list)
{
	uint32_t len = list.size();
	out.out.write((const char*)&len, sizeof(len));
	for (auto &obj : list) {
		if (!write_obj(out, obj))
			return false;
	}
	return true;
}

static bool
write_stringlist(SerialOut &out, const std::vector<std::string> &list)
{
	uint32_t len = list.size();
	out.out.write((const char*)&len, sizeof(len));
	for (auto &s : list)
		out <= s;
	return true;
}
static bool
write_stringset(SerialOut &out, const StringSet &list)
{
	uint32_t len = list.size();
	out.out.write((const char*)&len, sizeof(len));
	for (auto &s : list)
		out <= s;
	return true;
}

static bool
write_obj(SerialOut &out, Elf *obj)
{
	// check if the object has already been serialized
	auto prev = out.objs.find(obj);
	if (prev != out.objs.end()) {
		out <= ObjRef::OBJREF <= prev->second;
		return true;
	}

	// OBJ ObjRef; and remember our pointer in the ObjOutMap
	out <= ObjRef::OBJ;
	out.objs[obj] = out.out.tellp();

	// Serialize the actual object data
	out <= obj->dirname
	    <= obj->basename
	    <= obj->ei_class
	    <= obj->ei_osabi
	    <= (uint8_t)obj->rpath_set
	    <= (uint8_t)obj->runpath_set
	    <= obj->rpath
	    <= obj->runpath;

	if (!write_stringlist(out, obj->needed))
		return false;

	return true;
}

static bool
write_pkg(SerialOut &out, Package *pkg)
{
	// check if the package has already been serialized
	auto prev = out.pkgs.find(pkg);
	if (prev != out.pkgs.end()) {
		out <= ObjRef::PKGREF <= prev->second;
		return true;
	}

	// PKG ObjRef; and remember our pointer in the PkgOutMap
	out <= ObjRef::PKG;
	out.pkgs[pkg] = out.out.tellp();

	// Now serialize the actual package data:
	out <= pkg->name;
	if (!write_objlist(out, pkg->objects))
		return false;
	return true;
}

static bool
db_store(DB *db, const std::string& filename)
{
	SerialOut out(db, filename);
	if (!out.out) {
		log(Error, "failed to open output file\n");
		return false;
	}

	out <= (uint32_t)db->packages.size();
	for (auto &pkg : db->packages) {
		if (!write_pkg(out, pkg))
			return false;
	}

	if (!write_objlist(out, db->objects))
		return false;

	out <= (uint32_t)db->required_found.size();
	for (auto &found : db->required_found) {
		if (!write_obj(out, found.first))
			return false;
		if (!write_objset(out, found.second))
			return false;
	}

	out <= (uint32_t)db->required_missing.size();
	for (auto &missing : db->required_missing) {
		if (!write_obj(out, missing.first))
			return false;
		if (!write_stringset(out, missing.second))
			return false;
	}

	return out.out;
}


// There we go:

bool
DB::store(const std::string& filename)
{
	return db_store(this, filename);
}
