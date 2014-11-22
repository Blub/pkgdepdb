package PKGDepDB::Package;

use strict;
use warnings;
use PKGDepDB;
use PKGDepDB::cmod;
use PKGDepDB::CVal;

sub new {
  my ($class, $ptr, $linked) = @_;

  unless ($ptr) {
    $ptr = PKGDepDB::cmod::pkgdepdb_pkg_new();
    $linked = 0;
  }

  my $self = bless { ptr    => $ptr,
                     linked => $linked  },
                   $class;
  return $self;
}

sub DESTROY {
  my $self = shift;
  return if ${^GLOBAL_PHASE} eq 'DESTRUCT';
  unless ($self->{linked}) {
    PKGDepDB::cmod::pkgdepdb_pkg_delete($self->{ptr});
  }
}

1;
