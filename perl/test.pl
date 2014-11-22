#!/usr/bin/perl

use strict;
use warnings;
use Test;

BEGIN {
  plan tests => 1;
}

use PKGDepDB::Cfg qw(LOG_LEVEL_WARN JSONBITS_QUERY);
use PKGDepDB::Db;

my $cfg = PKGDepDB::Cfg->new();
ok($cfg->read("test",
"quiet = true
verbose = false
package_depends = true
package_file_lists = true
package_info = true
max_jobs = 1
json = off"));

$cfg->{log_level} = LOG_LEVEL_WARN;
