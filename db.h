#ifndef PKGDEPDB_DB_H__
#define PKGDEPDB_DB_H__

namespace pkgdepdb {

struct DB {
  static uint16_t CURRENT;

  uint16_t                     loaded_version_;
  bool                         strict_linking_; // stored as flag bit

  string                       name_;
  StringList                   library_path_;

  PackageList                  packages_;
  ObjectList                   objects_;

  StringSet                    ignore_file_rules_;
  std::map<string, StringList> package_library_path_;
  StringSet                    base_packages_;
  StringSet                    assume_found_rules_;

// non-serialized {
  const Config&                config_;

  bool contains_package_depends_;
  bool contains_make_depends_;
  bool contains_check_depends_;
  bool contains_groups_;
  bool contains_filelists_;
// }

  DB() = delete;
  DB(const Config&);
  ~DB();
  DB(bool wiped, const DB& copy);


  bool InstallPackage(Package* &&pkg);
  bool DeletePackage (const string& name, bool destroy = true);
  bool DeletePackage (PackageList::const_iterator, bool destroy = true);
  Elf *FindFor       (const Elf*, const string& lib,
                      const StringList *extrapath) const;
  void LinkObject    (Elf*, const Package *owner,
                      ObjectSet &req_found,
                      StringSet &req_missing) const;
  void LinkObject_do (Elf*, const Package *owner);
  void RelinkAll     ();
  void FixPaths      ();
  bool WipePackages  ();
  bool WipeFilelists ();

#ifdef PKGDEPDB_ENABLE_THREADS
  void RelinkAll_Threaded();
#endif

  Package*                    FindPkg   (const string& name) const;
  PackageList::const_iterator FindPkg_i (const string& name) const;

  void ShowInfo();
  void ShowInfo_json();
  void ShowPackages     (bool filter_broken, bool filter_notempty,
                         const FilterList&, const ObjFilterList&);
  void ShowPackages_json(bool filter_broken, bool filter_notempty,
                         const FilterList&, const ObjFilterList&);
  void ShowObjects      (const FilterList&, const ObjFilterList&);
  void ShowObjects_json (const FilterList&, const ObjFilterList&);
  void ShowMissing      ();
  void ShowMissing_json ();
  void ShowFound        ();
  void ShowFound_json   ();
  void ShowFilelist     (const FilterList&, const StrFilterList&);
  void ShowFilelist_json(const FilterList&, const StrFilterList&);

  void CheckIntegrity(const FilterList &pkg_filters,
                      const ObjFilterList &obj_filters) const;
  void CheckIntegrity(const Package             *pkg,
                      const PkgMap              &pkgmap,
                      const PkgListMap          &providemap,
                      const PkgListMap          &replacemap,
                      const PkgMap              &basemap,
                      const ObjListMap          &objmap,
                      const vec<const Package*> &base,
                      const ObjFilterList       &obj_filters) const;

  bool Store(const string& filename);
  bool Load (const string& filename);
  bool Empty() const;

  bool LD_Append (const string& dir);
  bool LD_Prepend(const string& dir);
  bool LD_Delete (const string& dir);
  bool LD_Delete (size_t i);
  bool LD_Insert (const string& dir, size_t i);
  bool LD_Clear  ();

  bool IgnoreFile_Add   (const string& name);
  bool IgnoreFile_Delete(const string& name);
  bool IgnoreFile_Delete(size_t id);

  bool AssumeFound_Add   (const string& name);
  bool AssumeFound_Delete(const string& name);
  bool AssumeFound_Delete(size_t id);

  bool BasePackages_Add   (const string& name);
  bool BasePackages_Delete(const string& name);
  bool BasePackages_Delete(size_t id);

  bool PKG_LD_Insert(const string& pkg, const string& path, size_t);
  bool PKG_LD_Delete(const string& pkg, const string& path);
  bool PKG_LD_Delete(const string& pkg, size_t i);
  bool PKG_LD_Clear (const string& pkg);

  bool IsBroken(const Package *pkg) const;
  bool IsBroken(const Elf *elf) const;
  bool IsEmpty (const Package *elf, const ObjFilterList &filters) const;

 private:
  bool ElfFinds(const Elf*, const string& lib,
                const StringList *extrapath) const;

  const StringList* GetObjectLibPath(const Elf*) const;
  const StringList* GetPackageLibPath(const Package*) const;
};

bool db_store_json(DB *db, const string& filename);

} // ::pkgdepdb

#endif
