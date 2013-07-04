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

static inline SerialOut&
operator<=(SerialOut &out, size_t r)
{
	out.out.write((const char*)&r, sizeof(r));
	return out;
}

static inline SerialOut&
operator<=(SerialOut &out, ObjRef r)
{
	out.out.write((const char*)&r, sizeof(r));
	return out;
}

static inline SerialOut&
operator<=(SerialOut &out, const std::string& r)
{
	uint32_t len = r.length();
	out.out.write((const char*)&len, sizeof(len));
	out.out.write(r.c_str(), len);
	return out;
}

static bool commit_obj(SerialOut &out, Elf *obj);

static bool
commit_objlist(SerialOut &out, const ObjectList& list)
{
	uint32_t len = list.size();
	out.out.write((const char*)&len, sizeof(len));
	for (auto &obj : list) {
		if (!commit_obj(out, obj))
			return false;
	}
	return true;
}

static bool
commit_obj(SerialOut &out, Elf *obj)
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

	return true;
}

static bool
commit_pkg(SerialOut &out, Package *pkg)
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
	if (!commit_objlist(out, pkg->objects))
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

	for (auto &pkg : db->packages) {
		if (!commit_pkg(out, pkg))
			return false;
	}

	return true;
}


// There we go:

bool
DB::store(const std::string& filename)
{
	return db_store(this, filename);
}
