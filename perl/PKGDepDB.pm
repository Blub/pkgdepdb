package PKGDepDB;
use parent 'Exporter';
use strict;
use warnings;
use PKGDepDB::cmod;

sub init()     { PKGDepDB::cmod::pkgdepdb_init(); }
sub finalize() { PKGDepDB::cmod::pkgdepdb_finalize(); }
sub error()    { PKGDepDB::cmod::pkgdepdb_error(); }

our @EXPORT = qw(error);

BEGIN {
  init();
}

END {
  finalize();
}

1;
