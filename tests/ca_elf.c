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

START_TEST (test_ca_elf)
{
  pkgdepdb_elf libfoo = create_elf("/usr/lib", "libfoo.so",
                                 ELFCLASS64, ELFDATA2LSB, 0,
                                 "/usr/lib:/usr/local/lib",
                                 NULL,
                                 "/lib/ld-elf.so");
  if (!libfoo)
    ck_abort_msg("failed to create a simple elf object");

  pkgdepdb_elf_needed_add     (libfoo, "libbar1.so");
  pkgdepdb_elf_needed_add     (libfoo, "libbar2.so");
  ck_assert_int_eq(pkgdepdb_elf_needed_count(libfoo), 2);

  const char *needed[3];
  ck_assert_int_eq(pkgdepdb_elf_needed_get(libfoo, needed, 0, 3), 2);
  ck_assert_str_eq(needed[0], "libbar1.so");
  ck_assert_str_eq(needed[1], "libbar2.so");

  pkgdepdb_elf libbar1 = create_elf("/usr/lib", "libbar1.so",
                                 ELFCLASS64, ELFDATA2LSB, 0,
                                 "/usr/lib:/usr/local/lib",
                                 NULL,
                                 "/lib/ld-elf.so");

  pkgdepdb_elf libbar1_32 = create_elf("/usr/lib", "libbar1.so",
                                 ELFCLASS32, ELFDATA2LSB, 0,
                                 "/usr/lib:/usr/local/lib",
                                 NULL,
                                 "/lib/ld-elf.so");

  pkgdepdb_elf libbar1_abi = create_elf("/usr/lib", "libbar1.so",
                                 ELFCLASS64, ELFDATA2LSB, 1,
                                 "/usr/lib:/usr/local/lib",
                                 NULL,
                                 "/lib/ld-elf.so");

  ck_assert( pkgdepdb_elf_can_use(libfoo, libbar1, 1));
  ck_assert(!pkgdepdb_elf_can_use(libfoo, libbar1_32, 1));
  ck_assert( pkgdepdb_elf_can_use(libfoo, libbar1_abi, 0));
  ck_assert(!pkgdepdb_elf_can_use(libfoo, libbar1_abi, 1));
  ck_assert( pkgdepdb_elf_can_use(libbar1_abi, libfoo, 0));
  ck_assert(!pkgdepdb_elf_can_use(libbar1_abi, libfoo, 1));

  pkgdepdb_elf_unref(libbar1_abi);
  pkgdepdb_elf_unref(libbar1_32);
  pkgdepdb_elf_unref(libbar1);
  pkgdepdb_elf_unref(libfoo);
}
END_TEST

START_TEST (test_ca_libpkgdepdb)
{
  pkgdepdb_config *cfg = pkgdepdb_config_new();
  ck_assert(cfg);

  int err = 0;
  pkgdepdb_elf elf = pkgdepdb_elf_open(".libs/libpkgdepdb.so", &err, cfg);
  if (!elf) {
    if (err)
      ck_abort_msg("error reading libpkgdepdb.so - cannot read own shared library?");
    else
      perror("open");
    ck_abort_msg("failed to read libpkgdepdb.so: %s", pkgdepdb_error());
  }
  ck_assert_int_eq(err, 0);
  ck_assert_str_eq(pkgdepdb_elf_basename(elf), "libpkgdepdb.so");

  pkgdepdb_config_delete(cfg);
}
END_TEST

Suite *elf_suite() {
  Suite *s;
  TCase *tc_case;

  s = suite_create("capi_elf");
  tc_case = tcase_create("ca_elf");

  tcase_add_test(tc_case, test_ca_elf);
  tcase_add_test(tc_case, test_ca_libpkgdepdb);

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
