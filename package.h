#ifndef PKGDEPDB_PACKAGE_H__
#define PKGDEPDB_PACKAGE_H__

namespace pkgdepdb {

struct Package {
  string                  name_;
  string                  version_;
  vec<rptr<Elf>>          objects_;

  // DB version 3:
  DependList              depends_;
  DependList              optdepends_;
  DependList              makedepends_; // DB version 10
  DependList              checkdepends_; // DB version 12
  DependList              provides_;
  DependList              conflicts_;
  DependList              replaces_;
  // DB version 5:
  StringSet               groups_;
  // DB version 6:
  // the filelist includes object files in v6 - makes things easier
  StringList              filelist_;

  // DB version 11:
  string                  description_; // not stored
  string                  pkgbase_; // DB version 13
  // not stored:
  std::map<string,vec<string>>
                          info_; // generic info - everything not caught above

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
  static bool Conflict(const Package&, const Package&);
  bool ConflictsWith(const Package&) const;
  bool Replaces(const Package&) const;

  // Output function:
  void ShowNeeded();

  // parsing a .PKGINFO file in a string
  bool ReadInfo(const string&, const size_t size, const Config& optconfig);
};

} // ::pkgdepdb

#endif
