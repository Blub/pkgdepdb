#include <stdint.h>
#include <string.h>

#include <memory>
#include <algorithm>
#include <utility>

#include <fstream>

#include "main.h"

using std::ofstream;
using std::ifstream;

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
using PkgInMap  = std::map<size_t,   Package*>;
using ObjInMap  = std::map<size_t,   Elf*>;

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

class SerialIn {
public:
	DB        *db;
	ifstream   in;
	PkgInMap   pkgs;
	ObjInMap   objs;

	SerialIn(DB *db_, const std::string& filename)
	: db(db_), in(filename.c_str(), std::ios::binary)
	{
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

template<typename T>
static inline SerialIn&
operator>=(SerialIn &in, T& r)
{
	in.in.read((char*)&r, sizeof(r));
	return in;
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

static inline SerialIn&
operator>=(SerialIn &in, std::string& r)
{
	uint32_t len;
	in >= len;
	r.resize(len);
	in.in.read(&r[0], len);
	return in;
}

static bool write_obj(SerialOut &out, Elf       *obj);
static bool read_obj (SerialIn  &in,  rptr<Elf> &obj);

static bool
write_objlist(SerialOut &out, const ObjectList& list)
{
	uint32_t len = list.size();
	out.out.write((const char*)&len, sizeof(len));
	for (auto &obj : list) {
		if (!write_obj(out, obj))
			return false;
	}
	return out.out;
}

static bool
read_objlist(SerialIn &in, ObjectList& list)
{
	uint32_t len;
	in >= len;
	list.resize(len);
	for (size_t i = 0; i != len; ++i) {
		if (!read_obj(in, list[i]))
			return false;
	}
	return in.in;
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
	return out.out;
}

static bool
read_objset(SerialIn &in, ObjectSet& list)
{
	uint32_t len;
	in >= len;
	for (size_t i = 0; i != len; ++i) {
		rptr<Elf> obj;
		if (!read_obj(in, obj))
			return false;
		list.insert(obj);
	}
	return in.in;
}

static bool
write_stringlist(SerialOut &out, const std::vector<std::string> &list)
{
	uint32_t len = list.size();
	out.out.write((const char*)&len, sizeof(len));
	for (auto &s : list)
		out <= s;
	return out.out;
}

static bool
read_stringlist(SerialIn &in, std::vector<std::string> &list)
{
	uint32_t len;
	in >= len;
	list.resize(len);
	for (uint32_t i = 0; i != len; ++i)
		in >= list[i];
	return in.in;
}

static bool
write_stringset(SerialOut &out, const StringSet &list)
{
	uint32_t len = list.size();
	out.out.write((const char*)&len, sizeof(len));
	for (auto &s : list)
		out <= s;
	return out.out;
}

static bool
read_stringset(SerialIn &in, StringSet &list)
{
	uint32_t len;
	in >= len;
	for (uint32_t i = 0; i != len; ++i) {
		std::string str;
		in >= str;
		list.insert(std::move(str));
	}
	return in.in;
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
read_obj(SerialIn &in, rptr<Elf> &obj)
{
	ObjRef r;
	in >= r;
	size_t ref;
	if (r == ObjRef::OBJREF) {
		in >= ref;
		auto existing = in.objs.find(ref);
		if (existing == in.objs.end()) {
			log(Error, "db error: failed to find previously deserialized object\n");
			return false;
		}
		obj = existing->second;
		return true;
	}
	if (r != ObjRef::OBJ) {
		log(Error, "object expected, object-ref value: %u\n", (unsigned)r);
		return false;
	}

	// Remember the one we're constructing now:
	obj = new Elf;
	ref = in.in.tellg();
	in.objs[ref] = obj.get();

	// Read out the object data

	// Serialize the actual object data
	uint8_t rpset, runpset;
	in >= obj->dirname
	   >= obj->basename
	   >= obj->ei_class
	   >= obj->ei_osabi
	   >= rpset
	   >= runpset
	   >= obj->rpath
	   >= obj->runpath;
	obj->rpath_set   = rpset;
	obj->runpath_set = runpset;

	if (!read_stringlist(in, obj->needed))
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
read_pkg(SerialIn &in, Package *&pkg)
{
	ObjRef r;
	in >= r;
	size_t ref;
	if (r == ObjRef::PKGREF) {
		in >= ref;
		auto existing = in.pkgs.find(ref);
		if (existing == in.pkgs.end()) {
			log(Error, "db error: failed to find previously deserialized package\n");
			return false;
		}
		pkg = existing->second;
		return true;
	}
	if (r != ObjRef::PKG) {
		log(Error, "package expected, object-ref value: %u\n", (unsigned)r);
		return false;
	}

	// Remember the one we're constructing now:
	pkg = new Package;
	ref = in.in.tellg();
	in.pkgs[ref] = pkg;

	// Now serialize the actual package data:
	in >= pkg->name;
	if (!read_objlist(in, pkg->objects))
		return false;
	return true;
}

static bool
db_store(DB *db, const std::string& filename)
{
	SerialOut out(db, filename);
	if (!out.out) {
		log(Error, "failed to open output file %s for writing\n", filename.c_str());
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

static bool
db_read(DB *db, const std::string& filename)
{
	SerialIn in(db, filename);
	if (!in.in) {
		log(Error, "failed to open input file %s for reading\n", filename.c_str());
		return false;
	}

	char magic[sizeof(depdb_magic)];
	in.in.read(magic, sizeof(magic));
	if (memcmp(magic, depdb_magic, sizeof(magic)) != 0) {
		log(Error, "not a valid database file: %s\n", filename.c_str());
		return false;
	}

	uint32_t len;

	in >= len;
	db->packages.resize(len);
	for (uint32_t i = 0; i != len; ++i) {
		if (!read_pkg(in, db->packages[i]))
			return false;
	}

	if (!read_objlist(in, db->objects))
		return false;

	in >= len;
	for (uint32_t i = 0; i != len; ++i) {
		rptr<Elf> obj;
		ObjectSet oset;
		if (!read_obj(in, obj) ||
		    !read_objset(in, oset))
		{
			return false;
		}
		db->required_found[obj.get()] = std::move(oset);
	}

	in >= len;
	for (uint32_t i = 0; i != len; ++i) {
		rptr<Elf> obj;
		StringSet sset;
		if (!read_obj(in, obj) ||
		    !read_stringset(in, sset))
		{
			return false;
		}
		db->required_missing[obj.get()] = std::move(sset);
	}

	return true;
}


// There we go:

bool
DB::store(const std::string& filename)
{
	return db_store(this, filename);
}

bool
DB::read(const std::string& filename)
{
	if (!empty()) {
		log(Error, "internal usage error: DB::read on a non-empty db!\n");
		return false;
	}
	return db_read(this, filename);
}
