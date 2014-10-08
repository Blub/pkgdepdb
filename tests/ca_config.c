#include <check.h>

#include <stdlib.h>

#include "../pkgdepdb.h"

START_TEST (test_ca_config)
{
  pkgdepdb_cfg *cfg = pkgdepdb_cfg_new();
  if (!cfg)
    ck_abort_msg("failed to instantiate a config structure");

  pkgdepdb_cfg_set_database          (cfg, "test.db.gz");
  pkgdepdb_cfg_set_verbosity         (cfg, 3);
  pkgdepdb_cfg_set_quiet             (cfg, 1);
  pkgdepdb_cfg_set_package_depends   (cfg, 0);
  pkgdepdb_cfg_set_package_file_lists(cfg, 1);
  pkgdepdb_cfg_set_package_info      (cfg, 1);
  pkgdepdb_cfg_set_max_jobs          (cfg, 4);
  pkgdepdb_cfg_set_log_level         (cfg, PKGDEPDB_CFG_LOG_LEVEL_PRINT);
  pkgdepdb_cfg_set_json              (cfg, PKGDEPDB_JSONBITS_QUERY);
  ck_assert_str_eq(pkgdepdb_cfg_database(cfg),           "test.db.gz");
  ck_assert_int_eq(pkgdepdb_cfg_verbosity(cfg),          3);
  ck_assert_int_eq(pkgdepdb_cfg_quiet(cfg),              1);
  ck_assert_int_eq(pkgdepdb_cfg_package_depends(cfg),    0);
  ck_assert_int_eq(pkgdepdb_cfg_package_file_lists(cfg), 1);
  ck_assert_int_eq(pkgdepdb_cfg_package_info(cfg),       1);
  ck_assert_int_eq(pkgdepdb_cfg_max_jobs(cfg),           4);
  ck_assert_int_eq(pkgdepdb_cfg_log_level(cfg), PKGDEPDB_CFG_LOG_LEVEL_PRINT);
  ck_assert_int_eq(pkgdepdb_cfg_json(cfg),      PKGDEPDB_JSONBITS_QUERY);

  pkgdepdb_cfg_set_database          (cfg, "other.db.gz");
  pkgdepdb_cfg_set_verbosity         (cfg, 0);
  pkgdepdb_cfg_set_quiet             (cfg, 0);
  pkgdepdb_cfg_set_package_depends   (cfg, 1);
  pkgdepdb_cfg_set_package_file_lists(cfg, 0);
  pkgdepdb_cfg_set_package_info      (cfg, 0);
  pkgdepdb_cfg_set_max_jobs          (cfg, 1);
  pkgdepdb_cfg_set_log_level         (cfg, PKGDEPDB_CFG_LOG_LEVEL_DEBUG);
  pkgdepdb_cfg_set_json              (cfg, PKGDEPDB_JSONBITS_DB);
  ck_assert_str_eq(pkgdepdb_cfg_database(cfg),           "other.db.gz");
  ck_assert_int_eq(pkgdepdb_cfg_verbosity(cfg),          0);
  ck_assert_int_eq(pkgdepdb_cfg_quiet(cfg),              0);
  ck_assert_int_eq(pkgdepdb_cfg_package_depends(cfg),    1);
  ck_assert_int_eq(pkgdepdb_cfg_package_file_lists(cfg), 0);
  ck_assert_int_eq(pkgdepdb_cfg_package_info(cfg),       0);
  ck_assert_int_eq(pkgdepdb_cfg_max_jobs(cfg),           1);
  ck_assert_int_eq(pkgdepdb_cfg_log_level(cfg), PKGDEPDB_CFG_LOG_LEVEL_DEBUG);
  ck_assert_int_eq(pkgdepdb_cfg_json(cfg),
                   PKGDEPDB_JSONBITS_DB);

  pkgdepdb_cfg_delete(cfg);
}
END_TEST

Suite *config_suite() {
  Suite *s;
  TCase *tc_config;

  s = suite_create("capi_config");
  tc_config = tcase_create("ca_config");

  tcase_add_test(tc_config, test_ca_config);

  suite_add_tcase(s, tc_config);

  return s;
}

int main(void) {
  int number_failed = 0;
  Suite *s;
  SRunner *sr;

  s = config_suite();
  sr = srunner_create(s);

  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);

  return number_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
