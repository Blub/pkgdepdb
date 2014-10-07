#include <check.h>

#include <stdlib.h>
#include <elf.h>

#include "../pkgdepdb.h"

START_TEST (test_ca_elf)
{
  pkgdepdb_config *cfg = pkgdepdb_config_new();
  ck_assert(cfg);

  pkgdepdb_elf elf1 = pkgdepdb_elf_new();
  ck_assert(elf1);

  pkgdepdb_elf_set_dirname    (elf1, "usr/lib");
  pkgdepdb_elf_set_basename   (elf1, "libfoo.so");
  pkgdepdb_elf_set_class      (elf1, ELFCLASS64);
  pkgdepdb_elf_set_data       (elf1, ELFDATA2LSB);
  pkgdepdb_elf_set_osabi      (elf1, 0);
  pkgdepdb_elf_set_rpath      (elf1, "/usr/lib:/usr/local/lib");
  pkgdepdb_elf_set_runpath    (elf1, NULL);
  pkgdepdb_elf_set_interpreter(elf1, "/lib/ld-elf.so");
  pkgdepdb_elf_needed_add     (elf1, "libbar1.so");
  pkgdepdb_elf_needed_add     (elf1, "libbar2.so");

  ck_assert_str_eq(pkgdepdb_elf_dirname(elf1),     "usr/lib");
  ck_assert_str_eq(pkgdepdb_elf_basename(elf1),    "libfoo.so");
  ck_assert_int_eq(pkgdepdb_elf_class(elf1),       ELFCLASS64);
  ck_assert_int_eq(pkgdepdb_elf_data(elf1),        ELFDATA2LSB);
  ck_assert_int_eq(pkgdepdb_elf_osabi(elf1),       0);
  ck_assert_str_eq(pkgdepdb_elf_rpath(elf1),       "/usr/lib:/usr/local/lib");
  ck_assert       (pkgdepdb_elf_runpath(elf1) == NULL);
  ck_assert_str_eq(pkgdepdb_elf_interpreter(elf1), "/lib/ld-elf.so");
  ck_assert_int_eq(pkgdepdb_elf_needed_count(elf1), 2);

  const char *needed[2];
  ck_assert_int_eq(pkgdepdb_elf_needed_get(elf1, needed, 0, 2), 2);
  ck_assert_str_eq(needed[0], "libbar1.so");
  ck_assert_str_eq(needed[1], "libbar2.so");

  pkgdepdb_elf_unref(elf1);

  pkgdepdb_config_delete(cfg);
}
END_TEST

Suite *elf_suite() {
  Suite *s;
  TCase *tc_case;

  s = suite_create("capi_elf");
  tc_case = tcase_create("ca_elf");

  tcase_add_test(tc_case, test_ca_elf);

  suite_add_tcase(s, tc_case);

  return s;
}

int main(void) {
  int number_failed = 0;
  Suite *s;
  SRunner *sr;

  s = elf_suite();
  sr = srunner_create(s);

  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);

  return number_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
