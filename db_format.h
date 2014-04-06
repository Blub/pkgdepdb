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
  virtual ssize_t Write(const void *buf, size_t bytes) = 0;
  virtual ssize_t Read (void *buf,       size_t bytes) = 0;
  virtual size_t  TellP() const = 0;
  virtual size_t  TellG() const = 0;

  virtual operator bool() const = 0;

  enum InOut {
    in, out
  };
};

class SerialIn {
 public:
  DB                           *db_;
  SerialStream                 &in_;
  std::unique_ptr<SerialStream> in_owning_;
  PkgInMap                      old_pkgref_;
  ObjInMap                      old_objref_;

  std::vector<Elf*>             objref_;
  std::vector<Package*>         pkgref_;
  // whether objref and pkgref are used
  bool                          ver8_refs_;

 private:
  SerialIn(DB*, SerialStream*);

 public:
  static SerialIn* Open(DB *db, const std::string& file, bool gz);
};

class SerialOut {
 public:
  DB                           *db_;
  SerialStream                 &out_;
  std::unique_ptr<SerialStream> out_owning_;

  std::map<const Elf*,    size_t> objref_;
  std::map<const Package*,size_t> pkgref_;

  bool GetObjRef(const Elf*,     size_t *out);
  bool GetPkgRef(const Package*, size_t *out);

 private:
  SerialOut(DB*, SerialStream*);

 public:
  static SerialOut* Open(DB *db, const std::string& file, bool gz);
};

template<typename T>
static inline SerialOut& operator<=(SerialOut &out, const T& r) {
  out.out_.Write((const char*)&r, sizeof(r));
  return out;
}

template<typename T>
static inline SerialIn& operator>=(SerialIn &in, T& r) {
  in.in_.Read((char*)&r, sizeof(r));
  return in;
}

// special for strings:
static inline SerialOut& operator<=(SerialOut &out, const std::string& r) {
  auto len = static_cast<uint32_t>(r.length());
  out.out_.Write((const char*)&len, sizeof(len));
  out.out_.Write(r.c_str(), len);
  return out;
}

static inline SerialIn& operator>=(SerialIn &in, std::string& r) {
  uint32_t len;
  in >= len;
  r.resize(len);
  in.in_.Read(&r[0], len);
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
