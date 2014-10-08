#include <check.h>

#include <stdio.h>
#include <stdlib.h>
#include <elf.h>

#include "../pkgdepdb.h"

static
pkgdepdb_elf create_elf(const char *dirname, const char *basename,
                        unsigned char elfclass, unsigned char elfdata,
                        unsigned char elfosabi,
                        const char *rpath,
                        const char *runpath,
                        const char *interpreter)
{
  pkgdepdb_elf obj = pkgdepdb_elf_new();
  if (!obj)
    return NULL;

  pkgdepdb_elf_set_dirname    (obj, dirname);
  pkgdepdb_elf_set_basename   (obj, basename);
  pkgdepdb_elf_set_class      (obj, elfclass);
  pkgdepdb_elf_set_data       (obj, elfdata);
  pkgdepdb_elf_set_osabi      (obj, elfosabi);
  pkgdepdb_elf_set_rpath      (obj, rpath);
  pkgdepdb_elf_set_runpath    (obj, runpath);
  pkgdepdb_elf_set_interpreter(obj, interpreter);

  ck_assert_str_eq(pkgdepdb_elf_dirname(obj),     dirname);
  ck_assert_str_eq(pkgdepdb_elf_basename(obj),    basename);
  ck_assert_int_eq(pkgdepdb_elf_class(obj),       elfclass);
  ck_assert_int_eq(pkgdepdb_elf_data(obj),        elfdata);
  ck_assert_int_eq(pkgdepdb_elf_osabi(obj),       elfosabi);
  ck_assert_int_eq(pkgdepdb_elf_needed_count(obj), 0);

  const char *s = pkgdepdb_elf_rpath(obj);
  if (s && rpath)
    ck_assert_str_eq(s, rpath);
  else
    ck_assert(!s && !rpath);

  s = pkgdepdb_elf_runpath(obj);
  if (runpath)
    ck_assert_str_eq(s, runpath);
  else
    ck_assert(!s && !runpath);

  s = pkgdepdb_elf_interpreter(obj);
  if (interpreter)
    ck_assert_str_eq(s, interpreter);
  else
    ck_assert(!s && !interpreter);

  return obj;
}

static pkgdepdb_elf elf_libfoo() {
  pkgdepdb_elf elf = create_elf("/usr/lib", "libfoo.so",
                                ELFCLASS64, ELFDATA2LSB, 0,
                                "/usr/lib:/usr/local/lib",
                                NULL, "/lib/ld-elf.so");
  pkgdepdb_elf_needed_add(elf, "libbar1.so");
  pkgdepdb_elf_needed_add(elf, "libbar2.so");
  return elf;
}

static pkgdepdb_elf elf_libbar1() {
  return create_elf("/usr/lib", "libbar1.so", ELFCLASS64, ELFDATA2LSB, 0,
                    "/usr/lib:/usr/local/lib",
                    NULL, "/lib/ld-elf.so");
}

static pkgdepdb_elf elf_libbar2() {
  return create_elf("/usr/lib", "libbar2.so", ELFCLASS64, ELFDATA2LSB, 0,
                    "/usr/lib:/usr/local/lib",
                    NULL, "/lib/ld-elf.so");
}

static pkgdepdb_pkg *pkg_libfoo() {
  pkgdepdb_pkg *pkg = pkgdepdb_pkg_new();
  ck_assert(pkg);

  pkgdepdb_elf elf = elf_libfoo();
  ck_assert(elf);

  pkgdepdb_pkg_set_name    (pkg, "libfoo");
  pkgdepdb_pkg_set_version (pkg, "1.0-1");
  pkgdepdb_pkg_dep_add     (pkg, PKGDEPDB_PKG_DEPENDS,     "libc", NULL);
  pkgdepdb_pkg_dep_add     (pkg, PKGDEPDB_PKG_DEPENDS,     "libbar1", NULL);
  pkgdepdb_pkg_dep_add     (pkg, PKGDEPDB_PKG_OPTDEPENDS,  "libbar2", ">1");
  pkgdepdb_pkg_dep_add     (pkg, PKGDEPDB_PKG_MAKEDEPENDS, "check", NULL);
  pkgdepdb_pkg_dep_add     (pkg, PKGDEPDB_PKG_CONFLICTS,   "foo", NULL);
  pkgdepdb_pkg_dep_add     (pkg, PKGDEPDB_PKG_REPLACES,    "foo", NULL);
  pkgdepdb_pkg_dep_add     (pkg, PKGDEPDB_PKG_PROVIDES,    "foo", "=1.0");
  pkgdepdb_pkg_groups_add  (pkg, "base");
  pkgdepdb_pkg_groups_add  (pkg, "foogroup");
  pkgdepdb_pkg_groups_add  (pkg, "devel");
  pkgdepdb_pkg_filelist_add(pkg, "usr/lib/libfoo.so");
  pkgdepdb_pkg_filelist_add(pkg, "usr/lib/libfoo.so.1");
  pkgdepdb_pkg_filelist_add(pkg, "usr/lib/libfoo.so.1.0");
  pkgdepdb_pkg_filelist_add(pkg, "usr/lib/libfoo.so.1.0.0");
  pkgdepdb_pkg_elf_add     (pkg, elf);

  pkgdepdb_elf_unref(elf);
  return pkg;
}

static pkgdepdb_pkg *pkg_libbar() {
  pkgdepdb_pkg *pkg = pkgdepdb_pkg_new();
  ck_assert(pkg);

  pkgdepdb_elf elf1 = elf_libbar1();
  pkgdepdb_elf elf2 = elf_libbar2();
  ck_assert(elf1);
  ck_assert(elf2);

  pkgdepdb_pkg_set_name    (pkg, "libbar");
  pkgdepdb_pkg_set_version (pkg, "1.0-1");
  pkgdepdb_pkg_filelist_add(pkg, "usr/lib/libbar1.so");
  pkgdepdb_pkg_filelist_add(pkg, "usr/lib/libbar2.so");
  pkgdepdb_pkg_elf_add     (pkg, elf1);
  pkgdepdb_pkg_elf_add     (pkg, elf2);

  pkgdepdb_elf_unref(elf2);
  pkgdepdb_elf_unref(elf1);
  return pkg;
}

START_TEST (test_ca_db)
{
  const char *paths[8];

  pkgdepdb_cfg *cfg = pkgdepdb_cfg_new();
  ck_assert(cfg);

  pkgdepdb_db *db = pkgdepdb_db_new(cfg);

  pkgdepdb_db_set_name          (db, "archpkg");
  pkgdepdb_db_set_strict_linking(db, 1);
  pkgdepdb_db_library_path_add  (db, "/lib");
  pkgdepdb_db_library_path_add  (db, "/usr/lib");
  ck_assert_str_eq(pkgdepdb_db_name(db), "archpkg");
  ck_assert_int_eq(pkgdepdb_db_strict_linking(db), 1);
  ck_assert_int_eq(pkgdepdb_db_library_path_count(db), 2);
  ck_assert_int_eq(pkgdepdb_db_library_path_get(db, paths, 0, 8), 2);

  pkgdepdb_pkg *libfoopkg = pkg_libfoo();
  ck_assert_int_eq(pkgdepdb_db_package_install(db, libfoopkg), 1);
  ck_assert_int_eq(pkgdepdb_db_package_count(db), 1);
  ck_assert_int_eq(pkgdepdb_db_package_is_broken(db, libfoopkg), 1);

  pkgdepdb_pkg *libbarpkg = pkg_libbar();
  ck_assert_int_eq(pkgdepdb_db_package_install(db, libbarpkg), 1);
  ck_assert_int_eq(pkgdepdb_db_package_count(db), 2);
  ck_assert_int_eq(pkgdepdb_db_package_is_broken(db, libfoopkg), 0);

  pkgdepdb_db_delete(db);
  pkgdepdb_cfg_delete(cfg);
}
END_TEST

Suite *db_suite() {
  Suite *s;
  TCase *tc_case;

  s = suite_create("capi_db");
  tc_case = tcase_create("ca_db");

  tcase_add_test(tc_case, test_ca_db);

  suite_add_tcase(s, tc_case);

  return s;
}

int main(void) {
  int number_failed = 0;
  Suite *s;
  SRunner *sr;

  s = db_suite();
  sr = srunner_create(s);

  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);

  return number_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
