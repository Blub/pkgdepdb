#ifndef PKGDEPDB_ELF_H__
#define PKGDEPDB_ELF_H__

namespace pkgdepdb {

struct Elf {
  size_t refcount_ = 0;

  // path + name separated
  string dirname_;
  string basename_;

  // classification:
  unsigned char ei_class_; // 32/64 bit
  unsigned char ei_data_;  // endianess
  unsigned char ei_osabi_; // freebsd/linux/...

  // requirements:
  bool        rpath_set_       = false;
  bool        runpath_set_     = false;
  bool        interpreter_set_ = false;
  string      rpath_;
  string      runpath_;
  string      interpreter_;
  vec<string> needed_;

// non-serialized {
  // not serialized INSIDE the object, but as part of the DB
  // (for compatibility with older database dumps)
  ObjectSet req_found_;
  StringSet req_missing_;

  // NOT SERIALIZED:
  struct {
    size_t id;
  } json_;

  Package *owner_;
// }


  Elf();
  Elf(const Elf& cp);
  static Elf* Open(const char* data, size_t size, bool *err, const char *name,
                   const Config&);

  // utility functions while loading
  void SolvePaths(const string& origin);
  bool CanUse(const Elf &other, bool strict) const;

  // utility functions for printing stuff
  const char *classString() const;
  const char *dataString()  const;
  const char *osabiString() const;
};


} // ::pkgdepdb

#endif
