#ifndef PKGDEPDB_PACKAGE_H__
#define PKGDEPDB_PACKAGE_H__

namespace pkgdepdb {

struct Package {
  string                  name_;
  string                  version_;
  vec<rptr<Elf>>          objects_;

  // DB version 3:
  StringList              depends_;
  StringList              optdepends_;
  StringList              makedepends_;
  StringList              provides_;
  StringList              conflicts_;
  StringList              replaces_;
  // DB version 5:
  StringSet               groups_;
  // DB version 6:
  // the filelist includes object files in v6 - makes things easier
  StringList              filelist_;

// non-serialized {
  // used only while loading an archive
  struct {
    std::map<string, string> symlinks;
  } load_;
// }


  static Package* Open(const string& path, const Config&);
  Elf* Find(const string &dirname, const string &basename) const;

  // loading utiltiy functions
  void Guess(const string& name);
  bool ConflictsWith(const Package&) const;
  bool Replaces(const Package&) const;

  // Output function:
  void ShowNeeded();
};

} // ::pkgdepdb

#endif
