#include <string.h>

#include <zlib.h>

#include <memory>
#include <algorithm>
#include <utility>

#include <fstream>
#include <sstream>

#include "main.h"

// version
uint16_t
DB::CURRENT = 3;

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
		BasePackages  = (1<<2)
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

class SerialOut {
public:
	DB           *db;
	std::ostream  out;
	PkgOutMap     pkgs;
	ObjOutMap     objs;

	SerialOut(DB *db_, const std::string& filename)
	: db(db_), out(new std::filebuf)
	{
		std::filebuf &buf = *reinterpret_cast<std::filebuf*>(out.rdbuf());
		if (!buf.open(filename, std::ios::out | std::ios::binary))
			out.clear(std::ios::failbit | std::ios::badbit);
	}

	SerialOut(DB *db_)
	: db(db_), out(new std::stringbuf)
	{
	}

	~SerialOut()
	{
		auto b = out.rdbuf();
		out.rdbuf(nullptr);
		delete b;
	}
};

class gz_in_buf : public std::streambuf {
private:
	bool   eof;
	gzFile file;

public:
	operator bool() const { return file; }

	explicit gz_in_buf(const std::string& filename)
	: eof(false), file(nullptr)
	{
		file = gzopen(filename.c_str(), "rb");
	}

	~gz_in_buf()
	{
		if (file)
			gzclose(file);
	}

protected:
	virtual int_type
	uflow() {
		if (eof) return traits_type::eof();
		int b = gzgetc(file);
		if (b == -1) {
			eof = true;
			return traits_type::eof();
		}
		return b;
	}

	virtual int_type underflow() { abort(); }
	virtual int_type pbackfail(int_type c = traits_type::eof()) { (void)c; abort(); }

	virtual pos_type
	seekpos(pos_type off, std::ios_base::openmode mode)
	{
		if (!(mode & std::ios::in))
			return 0;
		if (mode & std::ios::in)
			return gzseek(file, off, SEEK_SET);
		return gztell(file);
	}

	virtual pos_type seekoff(off_type off, std::ios_base::seekdir way,
	                         std::ios_base::openmode mode)
	{
		if (!(mode & std::ios::in))
			return 0;
		if (way == std::ios::end)
			return -1;
		if (way == std::ios::beg)
			return gzseek(file, off, SEEK_SET);
		if (way == std::ios::cur)
			return gzseek(file, off, SEEK_CUR);
		return gztell(file);
	}

	virtual std::streamsize
	xsgetn(char *d, std::streamsize n)
	{
		if (!n)  return 0;
		if (eof) return traits_type::eof();
		int b = gzread(file, d, n);
		if (b == -1 || b == 0) {
			eof = true;
			return traits_type::eof();
		}
		return b;
	}
};

class SerialIn {
public:
	DB           *db;
	std::istream  in;
	PkgInMap      pkgs;
	ObjInMap      objs;

	SerialIn(DB *db_, const std::string& filename)
	: db(db_), in(new std::filebuf)
	{
		std::filebuf &buf = *reinterpret_cast<std::filebuf*>(in.rdbuf());
		if (!buf.open(filename, std::ios::in | std::ios::binary))
			in.clear(std::ios::failbit | std::ios::badbit);
	}

	SerialIn(DB *db_, std::streambuf *buf)
	: db(db_), in(buf)
	{
	}

	~SerialIn()
	{
		auto b = in.rdbuf();
		in.rdbuf(nullptr);
		delete b;
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
write_pkg(SerialOut &out, Package *pkg, bool depdata)
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

	if (depdata) {
		if (!write_stringlist(out, pkg->depends) ||
		    !write_stringlist(out, pkg->optdepends))
		{
			return false;
		}
	}
	return true;
}

static bool
read_pkg(SerialIn &in, Package *&pkg, bool depdata)
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

	if (depdata) {
		if (!read_stringlist(in, pkg->depends) ||
		    !read_stringlist(in, pkg->optdepends))
		{
			return false;
		}
	}
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
	std::unique_ptr<SerialOut> sout;
	bool mkgzip = ends_with_gz(filename);

	if (mkgzip)
		sout.reset(new SerialOut(db));
	else {
		log(Message, "writing database\n");
		sout.reset(new SerialOut(db, filename));
	}

	SerialOut &out(*sout);

	if (!out.out) {
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

	// Figure out which database format version this will be
	if (db->contains_package_depends)
		hdr.version = 3;
	else if (hdr.flags)
			hdr.version = 2;

	// okay

	out <= hdr;
	out <= db->name;
	if (!write_stringlist(out, db->library_path))
		return false;

	out <= (uint32_t)db->packages.size();
	for (auto &pkg : db->packages) {
		if (!write_pkg(out, pkg, hdr.version >= 3))
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

	if (mkgzip) {
		log(Message, "writing compressed database\n");
		gzFile gz = gzopen(filename.c_str(), "wb");
		if (!gz) {
			log(Error, "failed to open gzip output stream\n");
			return false;
		}
		std::stringbuf *buf = reinterpret_cast<std::stringbuf*>(out.out.rdbuf());
		std::string data = buf->str();
		if ((size_t)gzwrite(gz, &data[0], data.length()) != data.length()) {
			log(Error, "failed to write database to gzip stream\n");
			gzclose(gz);
			return false;
		}
		gzclose(gz);
	}

	return out.out;
}

static bool
db_read(DB *db, const std::string& filename)
{
	std::unique_ptr<SerialIn> sin;
	bool gzip = ends_with_gz(filename);

	if (gzip) {
		log(Message, "reading compressed database\n");
		auto buf = new gz_in_buf(filename);
		if (!*buf) {
			delete buf;
			return true;
		}
		sin.reset(new SerialIn(db, buf));
	}
	else {
		log(Message, "reading database\n");
		sin.reset(new SerialIn(db, filename));
	}

	SerialIn &in(*sin);
	if (!in.in) {
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

	in >= db->name;
	if (!read_stringlist(in, db->library_path)) {
		log(Error, "failed reading library paths\n");
		return false;
	}

	uint32_t len;

	in >= len;
	db->packages.resize(len);
	for (uint32_t i = 0; i != len; ++i) {
		if (!read_pkg(in, db->packages[i], hdr.version >= 3)) {
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
