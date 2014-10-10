#!/usr/bin/python

# when running from the pkgdepdb source tree let PYTHONPATH point to the
# local pypkgdepdb/ and LD_LIBRARY_PATH point to .libs/

# eg:
# $ LD_LIBRARY_PATH=.libs PYTHONPATH=. python examples/report_broken_libs.py

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
    print('contains %i elfs:' % len(db.elfs))
    for obj in db.elfs:
        name = obj.basename
        for m in obj.missing:
            print('%s misses: %s' % (name, m))

except pypkgdepdb.PKGDepDBException as err:
    print('error: %s' % (str(err)))
    sys.exit(1)

del db
del cfg
sys.exit(0)
