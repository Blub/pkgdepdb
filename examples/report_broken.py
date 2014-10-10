#!/usr/bin/python

# when running from the pkgdepdb source tree let PYTHONPATH to point to the
# local pypkgdepdb/ and LD_LIBRARY_PATH point to .libs/

import sys
import pypkgdepdb

cfg = pypkgdepdb.Config()
cfg.load_default()

if len(cfg.database) == 0:
    print('No default database configured.')
    sys.exit(1)

# disable spam:
cfg.quiet = True
cfg.verbosity = 0
cfg.log_level = pypkgdepdb.LogLevel.Error

db = pypkgdepdb.DB(cfg)
try:
    print('loading database: %s' % (cfg.database))
    db.load(cfg.database)
    for pkg in db.packages:
        if db.is_broken(pkg):
            print(pkg.name)

except pypkgdepdb.PKGDepDBException as err:
    print('error: %s' % (str(err)))
    sys.exit(1)

del db
del cfg
sys.exit(0)
