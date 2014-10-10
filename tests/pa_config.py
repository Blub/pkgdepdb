import unittest
import ctypes

import pypkgdepdb

class TestConfig(unittest.TestCase):
    def setUp(self):
        self.cfg = pypkgdepdb.Config()
    def tearDown(self):
        del self.cfg

    def test_values(self):
        self.cfg.database           = 'test.db.gz'
        self.cfg.verbosity          = 3
        self.cfg.quiet              = True
        self.cfg.package_depends    = False
        self.cfg.package_file_lists = True
        self.cfg.package_info       = True
        self.cfg.max_jobs           = 3
        self.cfg.log_level          = pypkgdepdb.LogLevel.Print
        self.cfg.json               = pypkgdepdb.JSON.Query
        self.assertEqual(self.cfg.database,          'test.db.gz')
        self.assertEqual(self.cfg.verbosity,         3)
        self.assertEqual(self.cfg.quiet,             True)
        self.assertEqual(self.cfg.package_depends,   False)
        self.assertEqual(self.cfg.package_file_lists,True)
        self.assertEqual(self.cfg.package_info,      True)
        self.assertEqual(self.cfg.max_jobs,          3)
        self.assertEqual(self.cfg.log_level,         pypkgdepdb.LogLevel.Print)
        self.assertEqual(self.cfg.json,              pypkgdepdb.JSON.Query)

    def test_parsing(self):
        self.cfg.read('test.cfg', '''
database = test2.db.gz
verbosity = 1
quiet = false
package_depends = true
file_lists = false
package_info = false
jobs = 1
json = off
''')
        self.assertEqual(self.cfg.database,           'test2.db.gz')
        self.assertEqual(self.cfg.verbosity,          1)
        self.assertEqual(self.cfg.quiet,              False)
        self.assertEqual(self.cfg.package_depends,    True)
        self.assertEqual(self.cfg.package_file_lists, False)
        self.assertEqual(self.cfg.package_info,       False)
        self.assertEqual(self.cfg.max_jobs,           1)
        self.assertEqual(self.cfg.json,               0)

if __name__ == '__main__':
    unittest.main()
