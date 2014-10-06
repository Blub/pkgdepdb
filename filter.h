#ifndef PKGDEPDB_FILTER_H__
#define PKGDEPDB_FILTER_H__

namespace pkgdepdb {

namespace filter {

class Match {
 public:
  size_t refcount_ = 0; // make it a capturable rptr
  Match();
  virtual ~Match();
  virtual bool operator()(const string&) const = 0;

  static rptr<Match> CreateExact(string &&text);
  static rptr<Match> CreateGlob (string &&text);
#ifdef PKGDEPDB_ENABLE_REGEX
  static rptr<Match> CreateRegex(string &&text, bool icase);
#endif
};

class PackageFilter {
 protected:
  PackageFilter(bool);
 public:
  PackageFilter() = delete;

  bool negate_;
  virtual ~PackageFilter();
  virtual bool visible(const Package &pkg) const {
    (void)pkg; return true;
  }
  virtual bool visible(const DB& db, const Package &pkg) const {
    (void)db; return visible(pkg);
  }
  inline bool operator()(const DB& db, const Package &pkg) const {
    return visible(db, pkg) != negate_;
  }

  static uniq<PackageFilter> name         (rptr<Match>, bool neg);
  static uniq<PackageFilter> group        (rptr<Match>, bool neg);
  static uniq<PackageFilter> depends      (rptr<Match>, bool neg);
  static uniq<PackageFilter> makedepends  (rptr<Match>, bool neg);
  static uniq<PackageFilter> optdepends   (rptr<Match>, bool neg);
  static uniq<PackageFilter> alldepends   (rptr<Match>, bool neg);
  static uniq<PackageFilter> provides     (rptr<Match>, bool neg);
  static uniq<PackageFilter> conflicts    (rptr<Match>, bool neg);
  static uniq<PackageFilter> replaces     (rptr<Match>, bool neg);
  static uniq<PackageFilter> contains     (rptr<Match>, bool neg);
  static uniq<PackageFilter> pkglibdepends(rptr<Match>, bool neg);
  static uniq<PackageFilter> pkglibrpath  (rptr<Match>, bool neg);
  static uniq<PackageFilter> pkglibrunpath(rptr<Match>, bool neg);
  static uniq<PackageFilter> pkglibinterp (rptr<Match>, bool neg);

  static uniq<PackageFilter> broken       (bool neg);
};

class ObjectFilter {
 protected:
  ObjectFilter(bool);
 public:
  ObjectFilter() = delete;

  size_t refcount_ = 0;
  bool   negate_;

  virtual ~ObjectFilter();
  virtual bool visible(const Elf &elf) const {
    (void)elf; return true;
  }
  virtual bool visible(const DB& db, const Elf &elf) const {
    (void)db; return visible(elf);
  }
  inline bool operator()(const DB& db, const Elf &elf) const {
    return visible(db, elf) != negate_;
  }

  static uniq<ObjectFilter> name   (rptr<Match>, bool neg);
  static uniq<ObjectFilter> path   (rptr<Match>, bool neg);
  static uniq<ObjectFilter> depends(rptr<Match>, bool neg);
  static uniq<ObjectFilter> rpath(rptr<Match>, bool neg);
  static uniq<ObjectFilter> runpath(rptr<Match>, bool neg);
  static uniq<ObjectFilter> interp(rptr<Match>, bool neg);
};

class StringFilter {
 protected:
  StringFilter(bool);
 public:
  StringFilter() = delete;

  bool negate_;
  virtual ~StringFilter();

  virtual bool visible(const string& str) const {
    (void)str; return true;
  }
  inline bool operator()(const string &str) const {
    return visible(str) != negate_;
  }

  static uniq<StringFilter> filter(rptr<Match>, bool neg);
};

} // ::pkgdepdb::filter

} // ::pkgdepdb

#endif
