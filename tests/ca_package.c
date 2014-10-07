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

  pkgdepdb_pkg_set_name    (libfoo, "libfoo");
  pkgdepdb_pkg_set_version (libfoo, "1.0-1");
  ck_assert_str_eq(pkgdepdb_pkg_name(libfoo),    "libfoo");
  ck_assert_str_eq(pkgdepdb_pkg_version(libfoo), "1.0-1");

  pkgdepdb_pkg_dep_add     (libfoo, PKGDEPDB_PKG_DEPENDS,     "libc", NULL);
  pkgdepdb_pkg_dep_add     (libfoo, PKGDEPDB_PKG_DEPENDS,     "libbar1", NULL);
  pkgdepdb_pkg_dep_add     (libfoo, PKGDEPDB_PKG_OPTDEPENDS,  "libbar2", ">1");
  pkgdepdb_pkg_dep_add     (libfoo, PKGDEPDB_PKG_MAKEDEPENDS, "check", NULL);
  pkgdepdb_pkg_dep_add     (libfoo, PKGDEPDB_PKG_CONFLICTS,   "foo", NULL);
  pkgdepdb_pkg_dep_add     (libfoo, PKGDEPDB_PKG_REPLACES,    "foo", NULL);
  pkgdepdb_pkg_dep_add     (libfoo, PKGDEPDB_PKG_PROVIDES,    "foo", "=1.0");

  ck_assert_int_eq(pkgdepdb_pkg_dep_count(libfoo, PKGDEPDB_PKG_DEPENDS),    2);
  ck_assert_int_eq(pkgdepdb_pkg_dep_count(libfoo, PKGDEPDB_PKG_OPTDEPENDS), 1);
  ck_assert_int_eq(pkgdepdb_pkg_dep_count(libfoo, PKGDEPDB_PKG_MAKEDEPENDS),1);
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

  pkgdepdb_pkg_set_name    (oldfoo, "foo");
  pkgdepdb_pkg_set_version (oldfoo, "0.9-1");
  ck_assert_str_eq(pkgdepdb_pkg_name(oldfoo),    "foo");
  ck_assert_str_eq(pkgdepdb_pkg_version(oldfoo), "0.9-1");

  ck_assert( pkgdepdb_pkg_conflict(libfoo, oldfoo));
  ck_assert(!pkgdepdb_pkg_conflict(oldfoo, libfoo));
  ck_assert( pkgdepdb_pkg_replaces(libfoo, oldfoo));
  ck_assert(!pkgdepdb_pkg_replaces(oldfoo, libfoo));

  pkgdepdb_pkg_delete(oldfoo);
  pkgdepdb_pkg_delete(libfoo);
}
END_TEST

Suite *package_suite() {
  Suite *s;
  TCase *tc_case;

  s = suite_create("capi_package");
  tc_case = tcase_create("ca_package");

  tcase_add_test(tc_case, test_ca_package);

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
