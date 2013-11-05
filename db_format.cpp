#include <string.h>

#include <zlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>

#include <memory>
#include <algorithm>
#include <utility>

#include "main.h"
#include "db_format.h"

// version
uint16_t
DB::CURRENT = 6;

// magic header
static const char
depdb_magic[] = { 'A', 'r', 'c', 'h',
                  'B', 'S', 'D',  0,
                  'd', 'e', 'p', 's',
                  '~', 'D', 'B', '~' };

namespace DBFlags {
	enum {
		IgnoreRules   = (1<<0),
		PackageLDPath = (1<<1),
		BasePackages  = (1<<2),
		StrictLinking = (1<<3),
		AssumeFound   = (1<<4)
	};
}

using Header = struct {
	uint8_t   magic[sizeof(depdb_magic)];
	uint16_t  version;
	uint16_t  flags;
	uint8_t   reserved[22];
};

// Simple straight forward data serialization by keeping track
// of already-serialized objects.
// Lame, but effective.

enum class ObjRef : uint8_t {
	PKG,
	PKGREF,
	OBJ,
	OBJREF
};

class SerialFile : public SerialStream {
public:
	int  fd;
	bool err;
	size_t ppos, gpos;

	SerialFile(const std::string& file, InOut dir)
	: ppos(0), gpos(0)
	{
		int locktype;
		if (dir == SerialStream::out) {
			fd = ::open(file.c_str(), O_WRONLY | O_CREAT, 0644);
			locktype = LOCK_EX;
		}
		else {
			fd = ::open(file.c_str(), O_RDONLY);
			locktype = LOCK_SH;
		}
		if (fd < 0) {
			err = true;
			return;
		}
		err = (::flock(fd, locktype) != 0);
		if (err) {
			::close(fd);
		}
	}

	~SerialFile()
	{
		if (fd >= 0)
			::close(fd);
	}

	virtual operator bool() const {
		return fd >= 0 && !err;
	}

	virtual ssize_t
	write(const void *buf, size_t bytes)
	{
		auto r = ::write(fd, buf, bytes);
		ppos += r;
		return r;
	}

	virtual ssize_t
	read(void *buf, size_t bytes)
	{
		auto r = ::read(fd, buf, bytes);
		gpos += r;
		return r;
	}

	virtual size_t tellp() const {
		return ppos;
	}
	virtual size_t tellg() const {
		return gpos;
	}
};

class SerialGZ : public SerialStream {
public:
	gzFile out;
	bool   err;

	SerialGZ(const std::string& file, InOut dir)
	{
		int fd;
		int locktype;
		out = 0;
		if (dir == SerialStream::out) {
			fd = ::open(file.c_str(), O_WRONLY | O_CREAT, 0644);
			locktype = LOCK_EX;
		}
		else {
			fd = ::open(file.c_str(), O_RDONLY);
			locktype = LOCK_SH;
		}
		if (fd < 0) {
			err = true;
			return;
		}
		err = (::flock(fd, locktype) != 0);
		if (err) {
			::close(fd);
			err = true;
			return;
		}
		out = gzdopen(fd, (dir == SerialStream::out ? "wb" : "rb"));
		if (!out) {
			err = true;
			::close(fd);
		}
	}

	~SerialGZ()
	{
		if (out)
			gzclose(out);
	}

	virtual operator bool() const {
		return out && !err;
	}

	virtual ssize_t
	write(const void *buf, size_t bytes)
	{
		return gzwrite(out, buf, bytes);
	}

	virtual ssize_t
	read(void *buf, size_t bytes)
	{
		return gzread(out, buf, bytes);
	}

	virtual size_t tellp() const {
		return gztell(out);
	}
	virtual size_t tellg() const {
		return gztell(out);
	}
};

SerialIn::SerialIn(DB *db_, SerialStream *in__)
: db(db_), in(*in__), in_(in__)
{ }

SerialIn*
SerialIn::open(DB *db, const std::string& file, bool gz)
{
	SerialStream *in = gz ? (SerialStream*)new SerialGZ(file, SerialStream::in)
	                      : (SerialStream*)new SerialFile(file, SerialStream::in);
	if (!in) return 0;
	if (!*in) {
		delete in;
		return 0;
	}

	SerialIn *s = new SerialIn(db, in);
	return s;
}

SerialOut::SerialOut(DB *db_, SerialStream *out__)
: db(db_), out(*out__), out_(out__)
{ }

SerialOut*
SerialOut::open(DB *db, const std::string& file, bool gz)
{
	SerialStream *out = gz ? (SerialStream*)new SerialGZ(file, SerialStream::out)
	                       : (SerialStream*)new SerialFile(file, SerialStream::out);
	if (!out) return 0;
	if (!*out) {
		delete out;
		return 0;
	}

	SerialOut *s = new SerialOut(db, out);
	return s;
}

static bool write_obj(SerialOut &out, Elf       *obj);
static bool read_obj (SerialIn  &in,  rptr<Elf> &obj);

bool
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

bool
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

bool
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

bool
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

bool
write_stringlist(SerialOut &out, const std::vector<std::string> &list)
{
	uint32_t len = list.size();
	out.out.write((const char*)&len, sizeof(len));
	for (auto &s : list)
		out <= s;
	return out.out;
}

bool
read_stringlist(SerialIn &in, std::vector<std::string> &list)
{
	uint32_t len;
	in >= len;
	list.resize(len);
	for (uint32_t i = 0; i != len; ++i)
		in >= list[i];
	return in.in;
}

bool
write_stringset(SerialOut &out, const StringSet &list)
{
	uint32_t len = list.size();
	out.out.write((const char*)&len, sizeof(len));
	for (auto &s : list)
		out <= s;
	return out.out;
}

bool
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
	    <= obj->ei_data
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
	   >= obj->ei_data
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
write_pkg(SerialOut &out, Package *pkg, unsigned hdrver)
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
	out <= pkg->name
	    <= pkg->version;
	if (!write_objlist(out, pkg->objects))
		return false;

	if (hdrver >= 3) {
		if (!write_stringlist(out, pkg->depends) ||
		    !write_stringlist(out, pkg->optdepends))
		{
			return false;
		}
	}
	if (hdrver >= 4) {
		if (!write_stringlist(out, pkg->provides) ||
		    !write_stringlist(out, pkg->conflicts) ||
		    !write_stringlist(out, pkg->replaces))
		{
			return false;
		}
	}
	if (hdrver >= 5 && !write_stringset(out, pkg->groups))
		return false;
	return true;
}

static bool
read_pkg(SerialIn &in, Package *&pkg, unsigned hdrver)
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
	in >= pkg->name
	   >= pkg->version;
	if (!read_objlist(in, pkg->objects))
		return false;
	for (auto &o : pkg->objects)
		o->owner = pkg;

	if (hdrver >= 3) {
		if (!read_stringlist(in, pkg->depends) ||
		    !read_stringlist(in, pkg->optdepends))
		{
			return false;
		}
	}
	if (hdrver >= 4) {
		if (!read_stringlist(in, pkg->provides) ||
		    !read_stringlist(in, pkg->conflicts) ||
		    !read_stringlist(in, pkg->replaces))
		{
			return false;
		}
	}
	if (hdrver >= 5 && !read_stringset(in, pkg->groups))
		return false;
	return true;
}

static inline bool
ends_with_gz(const std::string& str)
{
	size_t pos = str.find_last_of('.');
	return (pos == str.length()-3 &&
	        str.compare(pos, 3, ".gz") == 0);
}

static bool
db_store(DB *db, const std::string& filename)
{
	bool mkgzip = ends_with_gz(filename);
	std::unique_ptr<SerialOut> sout(SerialOut::open(db, filename, mkgzip));

	if (mkgzip)
		log(Message, "writing compressed database\n");
	else
		log(Message, "writing database\n");

	SerialOut &out(*sout);

	if (!sout || !out.out) {
		log(Error, "failed to open output file %s for writing\n", filename.c_str());
		return false;
	}

	Header hdr;
	memset(&hdr, 0, sizeof(hdr));

	memcpy(hdr.magic, depdb_magic, sizeof(hdr.magic));
	hdr.version = 1;

	// flags:
	if (db->ignore_file_rules.size())
		hdr.flags |= DBFlags::IgnoreRules;
	if (db->package_library_path.size())
		hdr.flags |= DBFlags::PackageLDPath;
	if (db->base_packages.size())
		hdr.flags |= DBFlags::BasePackages;
	if (db->strict_linking)
		hdr.flags |= DBFlags::StrictLinking;
	if (db->assume_found_rules.size())
		hdr.flags |= DBFlags::AssumeFound;

	// Figure out which database format version this will be
	if (hdr.flags | DBFlags::AssumeFound)
		hdr.version = 6;
	else if (db->contains_groups)
		hdr.version = 5;
	else if (db->contains_package_depends)
		hdr.version = 4;
	else if (hdr.flags)
			hdr.version = 2;

	// okay

	out <= hdr;
	out <= db->name;
	if (!write_stringlist(out, db->library_path))
		return false;

	out <= (uint32_t)db->packages.size();
	for (auto &pkg : db->packages) {
		if (!write_pkg(out, pkg, hdr.version))
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

	if (hdr.flags & DBFlags::IgnoreRules) {
		if (!write_stringset(out, db->ignore_file_rules))
			return false;
	}
	if (hdr.flags & DBFlags::AssumeFound) {
		if (!write_stringset(out, db->assume_found_rules))
			return false;
	}

	if (hdr.flags & DBFlags::PackageLDPath) {
		out <= (uint32_t)db->package_library_path.size();
		for (auto iter : db->package_library_path) {
			out <= iter.first;
			if (!write_stringlist(out, iter.second))
				return false;
		}
	}

	if (hdr.flags & DBFlags::BasePackages) {
		if (!write_stringset(out, db->base_packages))
			return false;
	}

	return out.out;
}

static bool
db_read(DB *db, const std::string& filename)
{
	bool gzip = ends_with_gz(filename);
	std::unique_ptr<SerialIn> sin(SerialIn::open(db, filename, gzip));

	if (gzip)
		log(Message, "reading compressed database\n");
	else
		log(Message, "reading database\n");

	SerialIn &in(*sin);
	if (!sin || !in.in) {
		//log(Error, "failed to open input file %s for reading\n", filename.c_str());
		return true; // might not exist...
	}

	Header hdr;
	in >= hdr;
	if (memcmp(hdr.magic, depdb_magic, sizeof(hdr.magic)) != 0) {
		log(Error, "not a valid database file: %s\n", filename.c_str());
		return false;
	}

	db->loaded_version = hdr.version;
	// supported versions:
	if (hdr.version > DB::CURRENT)
	{
		log(Error, "cannot read depdb version %u files, (known up to %u)\n",
		    (unsigned)hdr.version,
		    (unsigned)DB::CURRENT);
		return false;
	}

	if (hdr.version >= 3)
		db->contains_package_depends = true;
	if (hdr.version >= 5)
		db->contains_groups = true;

	in >= db->name;
	if (!read_stringlist(in, db->library_path)) {
		log(Error, "failed reading library paths\n");
		return false;
	}

	uint32_t len;

	in >= len;
	db->packages.resize(len);
	for (uint32_t i = 0; i != len; ++i) {
		if (!read_pkg(in, db->packages[i], hdr.version)) {
			log(Error, "failed reading packages\n");
			return false;
		}
	}

	if (!read_objlist(in, db->objects)) {
		log(Error, "failed reading object list\n");
		return false;
	}

	in >= len;
	for (uint32_t i = 0; i != len; ++i) {
		rptr<Elf> obj;
		ObjectSet oset;
		if (!read_obj(in, obj) ||
		    !read_objset(in, oset))
		{
			log(Error, "failed reading map of found depdendencies\n");
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
			log(Error, "failed reading map of missing depdendencies\n");
			return false;
		}
		db->required_missing[obj.get()] = std::move(sset);
	}

	if (hdr.version < 2)
		return true;

	if (hdr.flags & DBFlags::IgnoreRules) {
		if (!read_stringset(in, db->ignore_file_rules))
			return false;
	}
	if (hdr.flags & DBFlags::AssumeFound) {
		if (!read_stringset(in, db->assume_found_rules))
			return false;
	}

	if (hdr.flags & DBFlags::PackageLDPath) {
		in >= len;
		for (uint32_t i = 0; i != len; ++i) {
			std::string pkg;
			in >= pkg;
			if (!read_stringlist(in, db->package_library_path[pkg]))
				return false;
		}
	}

	if (hdr.flags & DBFlags::BasePackages) {
		if (!read_stringset(in, db->base_packages))
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

bool
DB::read(const std::string& filename)
{
	if (!empty()) {
		log(Error, "internal usage error: DB::read on a non-empty db!\n");
		return false;
	}
	return db_read(this, filename);
}
