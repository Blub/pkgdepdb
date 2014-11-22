#!/usr/bin/perl

use strict;
use warnings;
use Test;

BEGIN {
  plan tests => 19;
}

use PKGDepDB::Cfg qw(LOG_LEVEL_WARN JSONBITS_QUERY);
use PKGDepDB::Db;

my $cfg = PKGDepDB::Cfg->new();

sub tryassign($$$) {
  my ($cfg, $what, $value) = @_;
  $cfg->{$what} = $value;
  ok($cfg->{$what}, $value);
}

my %values = (
  'database'           => '/my/db.gz',
  'verbosity'          => 3,
  'quiet'              => 1,
  'package_depends'    => 1,
  'package_file_lists' => 0,
  'package_info'       => 0,
  'max_jobs'           => 2,
  'log_level'          => LOG_LEVEL_WARN,
  'json'               => JSONBITS_QUERY,
  );

keys %values;
while (my ($k, $v) = each %values) {
  tryassign $cfg, $k, $v;
}

ok($cfg->read("test", "database = /stored/db.gz
package_info = true"));
$values{database} = '/stored/db.gz';
$values{package_info} = 1;
keys %values;
while (my ($k, $v) = each %values) {
  ok($cfg->{$k}, $v);
}
