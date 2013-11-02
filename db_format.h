#ifndef PKGDEPDB_DB_FORMAT_H__
#define PKGDEPDB_DB_FORMAT_H__

// requires main.h

using PkgOutMap = std::map<Package*, size_t>;
using ObjOutMap = std::map<Elf*,     size_t>;
using PkgInMap  = std::map<size_t,   Package*>;
using ObjInMap  = std::map<size_t,   Elf*>;

class SerialStream {
public:
	virtual ~SerialStream() {}
	virtual ssize_t write(const void *buf, size_t bytes) = 0;
	virtual ssize_t read (void *buf,       size_t bytes) = 0;
	virtual size_t  tellp() const = 0;
	virtual size_t  tellg() const = 0;

	virtual operator bool() const = 0;

	enum InOut {
		in, out
	};
};

class SerialIn {
public:
	DB                           *db;
	SerialStream                 &in;
	std::unique_ptr<SerialStream> in_;
	PkgInMap                      pkgs;
	ObjInMap                      objs;

private:
	SerialIn(DB*, SerialStream*);

public:
	static SerialIn* open(DB *db, const std::string& file, bool gz);
};

class SerialOut {
public:
	DB                           *db;
	SerialStream                 &out;
	std::unique_ptr<SerialStream> out_;
	PkgOutMap                     pkgs;
	ObjOutMap                     objs;

private:
	SerialOut(DB*, SerialStream*);

public:
	static SerialOut* open(DB *db, const std::string& file, bool gz);
};

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

bool write_objlist   (SerialOut &out, const ObjectList& list);
bool read_objlist    (SerialIn  &in,  ObjectList& list);
bool write_objset    (SerialOut &out, const ObjectSet& list);
bool read_objset     (SerialIn  &in,  ObjectSet& list);
bool write_stringlist(SerialOut &out, const std::vector<std::string> &list);
bool read_stringlist (SerialIn  &in,  std::vector<std::string> &list);
bool write_stringset (SerialOut &out, const StringSet &list);
bool read_stringset  (SerialIn  &in,  StringSet &list);

#endif