#include <check.h>

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

START_TEST (test_ca_elf)
{
  pkgdepdb_config *cfg = pkgdepdb_config_new();
  ck_assert(cfg);

  pkgdepdb_elf elf1 = create_elf("usr/lib", "libfoo.so",
                                 ELFCLASS64, ELFDATA2LSB, 0,
                                 "/usr/lib:/usr/local/lib",
                                 NULL,
                                 "/lib/ld-elf.so");
  ck_assert(elf1);

  pkgdepdb_elf_needed_add     (elf1, "libbar1.so");
  pkgdepdb_elf_needed_add     (elf1, "libbar2.so");
  ck_assert_int_eq(pkgdepdb_elf_needed_count(elf1), 2);

  const char *needed[2];
  ck_assert_int_eq(pkgdepdb_elf_needed_get(elf1, needed, 0, 2), 2);
  ck_assert_str_eq(needed[0], "libbar1.so");
  ck_assert_str_eq(needed[1], "libbar2.so");

  pkgdepdb_elf elf2 = create_elf("usr/lib", "libbar1.so",
                                 ELFCLASS64, ELFDATA2LSB, 0,
                                 "/usr/lib:/usr/local/lib",
                                 NULL,
                                 "/lib/ld-elf.so");

  pkgdepdb_elf elf3 = create_elf("usr/lib", "libbar1.so",
                                 ELFCLASS32, ELFDATA2LSB, 0,
                                 "/usr/lib:/usr/local/lib",
                                 NULL,
                                 "/lib/ld-elf.so");

  pkgdepdb_elf elf4 = create_elf("usr/lib", "libbar1.so",
                                 ELFCLASS64, ELFDATA2LSB, 1,
                                 "/usr/lib:/usr/local/lib",
                                 NULL,
                                 "/lib/ld-elf.so");

  ck_assert( pkgdepdb_elf_can_use(elf1, elf2, 1));
  ck_assert(!pkgdepdb_elf_can_use(elf1, elf3, 1));
  ck_assert( pkgdepdb_elf_can_use(elf1, elf4, 0));
  ck_assert(!pkgdepdb_elf_can_use(elf1, elf4, 1));
  ck_assert( pkgdepdb_elf_can_use(elf4, elf1, 0));
  ck_assert(!pkgdepdb_elf_can_use(elf4, elf1, 1));

  pkgdepdb_elf_unref(elf3);
  pkgdepdb_elf_unref(elf2);
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
