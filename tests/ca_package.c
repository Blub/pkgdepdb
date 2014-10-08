#include <check.h>

#include <stdlib.h>
#include <string.h>

#include "../pkgdepdb.h"

START_TEST (test_ca_package)
{
  size_t i;
  int    k;
  const char *deps[8];
  const char *vers[8];

  pkgdepdb_pkg *libfoo = pkgdepdb_pkg_new();
  ck_assert(libfoo);
  pkgdepdb_pkg *oldfoo = pkgdepdb_pkg_new();
  ck_assert(oldfoo);

  pkgdepdb_pkg_set_name       (libfoo, "libfoo");
  pkgdepdb_pkg_set_version    (libfoo, "1.0-1");
  pkgdepdb_pkg_set_pkgbase    (libfoo, "foobase");
  pkgdepdb_pkg_set_description(libfoo, "foo description");
  ck_assert_str_eq(pkgdepdb_pkg_name(libfoo),        "libfoo");
  ck_assert_str_eq(pkgdepdb_pkg_version(libfoo),     "1.0-1");
  ck_assert_str_eq(pkgdepdb_pkg_pkgbase(libfoo),     "foobase");
  ck_assert_str_eq(pkgdepdb_pkg_description(libfoo), "foo description");

  pkgdepdb_pkg_dep_add     (libfoo, PKGDEPDB_PKG_DEPENDS,     "libc", NULL);
  pkgdepdb_pkg_dep_add     (libfoo, PKGDEPDB_PKG_DEPENDS,     "libbar1", NULL);
  pkgdepdb_pkg_dep_add     (libfoo, PKGDEPDB_PKG_OPTDEPENDS,  "libbar2", ">1");
  pkgdepdb_pkg_dep_add     (libfoo, PKGDEPDB_PKG_MAKEDEPENDS, "make", NULL);
  pkgdepdb_pkg_dep_add     (libfoo, PKGDEPDB_PKG_CHECKDEPENDS,"check", NULL);
  pkgdepdb_pkg_dep_add     (libfoo, PKGDEPDB_PKG_CONFLICTS,   "foo", NULL);
  pkgdepdb_pkg_dep_add     (libfoo, PKGDEPDB_PKG_REPLACES,    "foo", NULL);
  pkgdepdb_pkg_dep_add     (libfoo, PKGDEPDB_PKG_PROVIDES,    "foo", "=1.0");

  ck_assert_int_eq(pkgdepdb_pkg_dep_count(libfoo, PKGDEPDB_PKG_DEPENDS),    2);
  ck_assert_int_eq(pkgdepdb_pkg_dep_count(libfoo, PKGDEPDB_PKG_OPTDEPENDS), 1);
  ck_assert_int_eq(pkgdepdb_pkg_dep_count(libfoo, PKGDEPDB_PKG_MAKEDEPENDS),1);
  ck_assert_int_eq(pkgdepdb_pkg_dep_count(libfoo, PKGDEPDB_PKG_CHECKDEPENDS),
                   1);
  ck_assert_int_eq(pkgdepdb_pkg_dep_count(libfoo, PKGDEPDB_PKG_CONFLICTS),  1);
  ck_assert_int_eq(pkgdepdb_pkg_dep_count(libfoo, PKGDEPDB_PKG_REPLACES),   1);
  ck_assert_int_eq(pkgdepdb_pkg_dep_count(libfoo, PKGDEPDB_PKG_PROVIDES),   1);

  ck_assert_int_eq(pkgdepdb_pkg_dep_get(libfoo, PKGDEPDB_PKG_DEPENDS,
                                        deps, vers, 0, 3),                  2);
  ck_assert_str_eq(deps[0], "libc");
  ck_assert_str_eq(vers[0], "");
  ck_assert_str_eq(deps[1], "libbar1");
  ck_assert_str_eq(vers[1], "");
  ck_assert_int_eq(pkgdepdb_pkg_dep_get(libfoo, PKGDEPDB_PKG_OPTDEPENDS,
                                        deps, vers, 0, 3),                  1);
  ck_assert_str_eq(deps[0], "libbar2");
  ck_assert_str_eq(vers[0], ">1");
  ck_assert_int_eq(pkgdepdb_pkg_dep_get(libfoo, PKGDEPDB_PKG_MAKEDEPENDS,
                                        deps, vers, 0, 3),                  1);
  ck_assert_str_eq(deps[0], "make");
  ck_assert_str_eq(vers[0], "");
  ck_assert_int_eq(pkgdepdb_pkg_dep_get(libfoo, PKGDEPDB_PKG_CHECKDEPENDS,
                                        deps, vers, 0, 3),                  1);
  ck_assert_str_eq(deps[0], "check");
  ck_assert_str_eq(vers[0], "");
  ck_assert_int_eq(pkgdepdb_pkg_dep_get(libfoo, PKGDEPDB_PKG_CONFLICTS,
                                        deps, vers, 0, 3),                  1);
  ck_assert_str_eq(deps[0], "foo");
  ck_assert_str_eq(vers[0], "");
  ck_assert_int_eq(pkgdepdb_pkg_dep_get(libfoo, PKGDEPDB_PKG_REPLACES,
                                        deps, vers, 0, 3),                  1);
  ck_assert_str_eq(deps[0], "foo");
  ck_assert_str_eq(vers[0], "");
  ck_assert_int_eq(pkgdepdb_pkg_dep_get(libfoo, PKGDEPDB_PKG_PROVIDES,
                                        deps, vers, 0, 3),                  1);
  ck_assert_str_eq(deps[0], "foo");
  ck_assert_str_eq(vers[0], "=1.0");

  pkgdepdb_pkg_groups_add  (libfoo, "base");
  pkgdepdb_pkg_groups_add  (libfoo, "foogroup");
  pkgdepdb_pkg_groups_add  (libfoo, "devel");
  ck_assert_int_eq(pkgdepdb_pkg_groups_count(libfoo), 3);
  ck_assert_int_eq(pkgdepdb_pkg_groups_get(libfoo, deps, 0, 8), 3);

  k = 0;
  for (i = 0; i != 3; ++i) {
    if (!strcmp(deps[i], "base"))          k |= 1;
    else if (!strcmp(deps[i], "foogroup")) k |= 2;
    else if (!strcmp(deps[i], "devel"))    k |= 4;
    else ck_abort_msg("test package in invalid group: %s", deps[i]);
  }
  if (k != 7)
    ck_abort_msg("not all test package groups have been found");

  pkgdepdb_pkg_filelist_add(libfoo, "usr/lib/libfoo.so");
  pkgdepdb_pkg_filelist_add(libfoo, "usr/lib/libfoo.so.1");
  pkgdepdb_pkg_filelist_add(libfoo, "usr/lib/libfoo.so.1.0");
  pkgdepdb_pkg_filelist_add(libfoo, "usr/lib/libfoo.so.1.0.0");
  ck_assert_int_eq(pkgdepdb_pkg_filelist_count(libfoo), 4);
  ck_assert_int_eq(pkgdepdb_pkg_filelist_get(libfoo, deps, 0, 8), 4);
  ck_assert_str_eq(deps[0], "usr/lib/libfoo.so");
  ck_assert_str_eq(deps[1], "usr/lib/libfoo.so.1");
  ck_assert_str_eq(deps[2], "usr/lib/libfoo.so.1.0");
  ck_assert_str_eq(deps[3], "usr/lib/libfoo.so.1.0.0");

  ck_assert_int_eq(pkgdepdb_pkg_info_count_keys(libfoo), 0);
  ck_assert_int_eq(pkgdepdb_pkg_info_count_values(libfoo, "nonsense"), 0);
  ck_assert_int_eq(pkgdepdb_pkg_info_add(libfoo, "license", "BSD"), 1);
  ck_assert_int_eq(pkgdepdb_pkg_info_count_keys(libfoo), 1);
  ck_assert_int_eq(pkgdepdb_pkg_info_count_values(libfoo, "license"), 1);
  ck_assert_int_eq(pkgdepdb_pkg_info_count_values(libfoo, "nonsense"), 0);
  ck_assert_int_eq(pkgdepdb_pkg_info_get_values(libfoo, "license",vers,0,8),1);
  ck_assert_str_eq(vers[0], "BSD");
  ck_assert_int_eq(pkgdepdb_pkg_info_add(libfoo, "A", "1"), 1);
  ck_assert_int_eq(pkgdepdb_pkg_info_add(libfoo, "A", "2"), 1);
  ck_assert_int_eq(pkgdepdb_pkg_info_add(libfoo, "A", "3"), 1);
  ck_assert_int_eq(pkgdepdb_pkg_info_count_keys(libfoo), 2);
  ck_assert_int_eq(pkgdepdb_pkg_info_count_values(libfoo, "license"), 1);
  ck_assert_int_eq(pkgdepdb_pkg_info_count_values(libfoo, "A"), 3);
  ck_assert_int_eq(pkgdepdb_pkg_info_del_i(libfoo, "license", 0), 1);
  ck_assert_int_eq(pkgdepdb_pkg_info_count_keys(libfoo), 1);
  ck_assert_int_eq(pkgdepdb_pkg_info_count_values(libfoo, "license"), 0);
  ck_assert_int_eq(pkgdepdb_pkg_info_count_values(libfoo, "A"), 3);
  ck_assert_int_eq(pkgdepdb_pkg_info_get_values(libfoo, "A", deps, 0, 8), 3);
  ck_assert_str_eq(deps[0], "1");
  ck_assert_str_eq(deps[1], "2");
  ck_assert_str_eq(deps[2], "3");
  ck_assert_int_eq(pkgdepdb_pkg_info_del_i(libfoo, "A", 1), 1);
  ck_assert_int_eq(pkgdepdb_pkg_info_get_values(libfoo, "A", deps, 0, 8), 2);
  ck_assert_str_eq(deps[0], "1");
  ck_assert_str_eq(deps[1], "3");
  ck_assert_int_eq(pkgdepdb_pkg_info_del_s(libfoo, "A", "3"), 1);
  ck_assert_int_eq(pkgdepdb_pkg_info_get_values(libfoo, "A", deps, 0, 8), 1);
  ck_assert_str_eq(deps[0], "1");

  /* second package for conflict test */
  pkgdepdb_pkg_set_name     (oldfoo, "foo");
  pkgdepdb_pkg_guess_version(oldfoo, "foo-0.9-1-x86_64.pkg.tar.xz");

  ck_assert_str_eq(pkgdepdb_pkg_name(oldfoo),    "foo");
  ck_assert_str_eq(pkgdepdb_pkg_version(oldfoo), "0.9-1");

  ck_assert( pkgdepdb_pkg_conflict(libfoo, oldfoo));
  ck_assert(!pkgdepdb_pkg_conflict(oldfoo, libfoo));
  ck_assert( pkgdepdb_pkg_replaces(libfoo, oldfoo));
  ck_assert(!pkgdepdb_pkg_replaces(oldfoo, libfoo));

  pkgdepdb_elf elf1 = pkgdepdb_elf_new();
  ck_assert(elf1);
  pkgdepdb_elf elf2 = pkgdepdb_elf_new();
  ck_assert(elf2);
  pkgdepdb_elf_set_basename(elf1, "lib1.so");
  pkgdepdb_elf_set_basename(elf2, "lib2.so");
  pkgdepdb_pkg_elf_add(libfoo, elf1);
  pkgdepdb_pkg_elf_add(libfoo, elf2);
  ck_assert_int_eq(pkgdepdb_pkg_elf_count(libfoo), 2);
  pkgdepdb_elf elfs[3] = { NULL };
  ck_assert_int_eq(pkgdepdb_pkg_elf_get(libfoo, elfs, 0, 3), 2);
  ck_assert(!elfs[2]);
  ck_assert_str_eq(pkgdepdb_elf_basename(elfs[0]), "lib1.so");
  ck_assert_str_eq(pkgdepdb_elf_basename(elfs[1]), "lib2.so");
  pkgdepdb_elf_unref(elfs[0]);
  pkgdepdb_elf_unref(elfs[1]);

  ck_assert_int_eq(pkgdepdb_pkg_elf_del_i(libfoo, 1), 1);
  ck_assert_int_eq(pkgdepdb_pkg_elf_get(libfoo, elfs, 0, 3), 1);
  ck_assert_str_eq(pkgdepdb_elf_basename(elfs[0]), "lib1.so");
  pkgdepdb_elf_unref(elfs[0]);

  pkgdepdb_elf_unref(elf2);
  pkgdepdb_elf_unref(elf1);
  pkgdepdb_pkg_delete(oldfoo);
  pkgdepdb_pkg_delete(libfoo);
}
END_TEST

START_TEST (test_ca_pkginfo)
{
  size_t i;
  int    k;
  const char *deps[8];
  const char *vers[8];

  pkgdepdb_pkg *pkg = pkgdepdb_pkg_new();
  ck_assert(pkg);

  static const char pkginfo[] =
"# Generated by hand\n"
"pkgname = libfoo\n"
"pkgbase = foobase\n"
"pkgver = 1.0-1\n"
"pkgdesc = test package\n"
"url = http://about:blank\n"
"builddate = 1411832227\n"
"packager = The test suite\n"
"size = 1024\n"
"size = 1025\n"
"arch = x86_64\n"
"license = BSD\n"
"conflict = foo\n"
"provides = foo=1.0\n"
"replaces = foo\n"
"depend = libc\n"
"depend = libbar1\n"
"optdepend = libbar2>1: for libbar\n"
"makedepend = make\n"
"checkdepend = check\n"
"group = base\n"
"group = devel\n"
"group = foogroup\n";

  pkgdepdb_cfg *cfg = pkgdepdb_cfg_new();
  ck_assert(cfg);
  pkgdepdb_pkg_read_info(pkg, pkginfo, sizeof(pkginfo)-1, cfg);
  pkgdepdb_cfg_delete(cfg);

  ck_assert_str_eq(pkgdepdb_pkg_name(pkg),    "libfoo");
  ck_assert_str_eq(pkgdepdb_pkg_version(pkg), "1.0-1");
  ck_assert_str_eq(pkgdepdb_pkg_pkgbase(pkg), "foobase");
  ck_assert_int_eq(pkgdepdb_pkg_dep_count(pkg, PKGDEPDB_PKG_DEPENDS),      2);
  ck_assert_int_eq(pkgdepdb_pkg_dep_count(pkg, PKGDEPDB_PKG_OPTDEPENDS),   1);
  ck_assert_int_eq(pkgdepdb_pkg_dep_count(pkg, PKGDEPDB_PKG_MAKEDEPENDS),  1);
  ck_assert_int_eq(pkgdepdb_pkg_dep_count(pkg, PKGDEPDB_PKG_CHECKDEPENDS), 1);
  ck_assert_int_eq(pkgdepdb_pkg_dep_count(pkg, PKGDEPDB_PKG_CONFLICTS),    1);
  ck_assert_int_eq(pkgdepdb_pkg_dep_count(pkg, PKGDEPDB_PKG_REPLACES),     1);
  ck_assert_int_eq(pkgdepdb_pkg_dep_count(pkg, PKGDEPDB_PKG_PROVIDES),     1);

  ck_assert_int_eq(pkgdepdb_pkg_dep_get(pkg, PKGDEPDB_PKG_DEPENDS,
                                        deps, vers, 0, 3),                  2);
  ck_assert_str_eq(deps[0], "libc");
  ck_assert_str_eq(vers[0], "");
  ck_assert_str_eq(deps[1], "libbar1");
  ck_assert_str_eq(vers[1], "");
  ck_assert_int_eq(pkgdepdb_pkg_dep_get(pkg, PKGDEPDB_PKG_OPTDEPENDS,
                                        deps, vers, 0, 3),                  1);
  ck_assert_str_eq(deps[0], "libbar2");
  ck_assert_str_eq(vers[0], ">1");
  ck_assert_int_eq(pkgdepdb_pkg_dep_get(pkg, PKGDEPDB_PKG_MAKEDEPENDS,
                                        deps, vers, 0, 3),                  1);
  ck_assert_str_eq(deps[0], "make");
  ck_assert_str_eq(vers[0], "");
  ck_assert_int_eq(pkgdepdb_pkg_dep_get(pkg, PKGDEPDB_PKG_CHECKDEPENDS,
                                        deps, vers, 0, 3),                  1);
  ck_assert_str_eq(deps[0], "check");
  ck_assert_str_eq(vers[0], "");
  ck_assert_int_eq(pkgdepdb_pkg_dep_get(pkg, PKGDEPDB_PKG_CONFLICTS,
                                        deps, vers, 0, 3),                  1);
  ck_assert_str_eq(deps[0], "foo");
  ck_assert_str_eq(vers[0], "");
  ck_assert_int_eq(pkgdepdb_pkg_dep_get(pkg, PKGDEPDB_PKG_REPLACES,
                                        deps, vers, 0, 3),                  1);
  ck_assert_str_eq(deps[0], "foo");
  ck_assert_str_eq(vers[0], "");
  ck_assert_int_eq(pkgdepdb_pkg_dep_get(pkg, PKGDEPDB_PKG_PROVIDES,
                                        deps, vers, 0, 3),                  1);
  ck_assert_str_eq(deps[0], "foo");
  ck_assert_str_eq(vers[0], "=1.0");

  ck_assert_int_eq(pkgdepdb_pkg_groups_count(pkg), 3);
  ck_assert_int_eq(pkgdepdb_pkg_groups_get(pkg, deps, 0, 8), 3);

  k = 0;
  for (i = 0; i != 3; ++i) {
    if      (!strcmp(deps[i], "base"))     k |= 1;
    else if (!strcmp(deps[i], "foogroup")) k |= 2;
    else if (!strcmp(deps[i], "devel"))    k |= 4;
    else ck_abort_msg("test package in invalid group: %s", deps[i]);
  }
  if (k != 7)
    ck_abort_msg("not all test package groups have been found");

  ck_assert_int_eq(pkgdepdb_pkg_info_count_keys(pkg), 6);
  ck_assert_int_eq(pkgdepdb_pkg_info_get_keys(pkg, deps, 0, 8), 6);

  k = 0;
  for (i = 0; i != 6; ++i) {
    if      (!strcmp(deps[i], "url"))       k |= 1;
    else if (!strcmp(deps[i], "builddate")) k |= 2;
    else if (!strcmp(deps[i], "packager"))  k |= 4;
    else if (!strcmp(deps[i], "size"))      k |= 8;
    else if (!strcmp(deps[i], "arch"))      k |= 16;
    else if (!strcmp(deps[i], "license"))   k |= 32;
    else ck_abort_msg("unexpected info key: `%s`", deps[i]);
  }
  if (k != 63)
    ck_abort_msg("not all unhandled info lines were stored (%x)", k);

  ck_assert_int_eq(pkgdepdb_pkg_info_count_values(pkg, "url"), 1);
  ck_assert_int_eq(pkgdepdb_pkg_info_count_values(pkg, "builddate"), 1);
  ck_assert_int_eq(pkgdepdb_pkg_info_count_values(pkg, "packager"), 1);
  ck_assert_int_eq(pkgdepdb_pkg_info_count_values(pkg, "size"), 2);
  ck_assert_int_eq(pkgdepdb_pkg_info_count_values(pkg, "arch"), 1);
  ck_assert_int_eq(pkgdepdb_pkg_info_count_values(pkg, "license"), 1);

  ck_assert_int_eq(pkgdepdb_pkg_info_get_values(pkg,"url",      vers, 0,8), 1);
  ck_assert_str_eq(vers[0], "http://about:blank");
  ck_assert_int_eq(pkgdepdb_pkg_info_get_values(pkg,"builddate",vers, 0,8), 1);
  ck_assert_str_eq(vers[0], "1411832227");
  ck_assert_int_eq(pkgdepdb_pkg_info_get_values(pkg,"packager", vers, 0,8), 1);
  ck_assert_str_eq(vers[0], "The test suite");
  ck_assert_int_eq(pkgdepdb_pkg_info_get_values(pkg,"size",     vers, 0,8), 2);
  ck_assert_str_eq(vers[0], "1024");
  ck_assert_str_eq(vers[1], "1025");
  ck_assert_int_eq(pkgdepdb_pkg_info_get_values(pkg,"arch",     vers, 0,8), 1);
  ck_assert_str_eq(vers[0], "x86_64");
  ck_assert_int_eq(pkgdepdb_pkg_info_get_values(pkg,"license",  vers, 0,8), 1);
  ck_assert_str_eq(vers[0], "BSD");

  pkgdepdb_pkg_delete(pkg);
}
END_TEST

Suite *package_suite() {
  Suite *s;
  TCase *tc_case;

  s = suite_create("capi_package");
  tc_case = tcase_create("ca_package");

  tcase_add_test(tc_case, test_ca_package);
  tcase_add_test(tc_case, test_ca_pkginfo);

  suite_add_tcase(s, tc_case);

  return s;
}

int main(void) {
  int number_failed = 0;
  Suite *s;
  SRunner *sr;

  s = package_suite();
  sr = srunner_create(s);

  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);

  return number_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
