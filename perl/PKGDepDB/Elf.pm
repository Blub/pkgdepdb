package PKGDepDB::Elf;

use strict;
use warnings;
use PKGDepDB;
use PKGDepDB::cmod;
use PKGDepDB::CVal;

sub new {
  my ($class, $ptr) = @_;

  unless ($ptr) {
    $ptr = PKGDepDB::cmod::pkgdepdb_elf_new();
  }

  my $self = bless { ptr => $ptr }, $class;
  return $self;
}

sub DESTROY {
  my $self = shift;
  return if ${^GLOBAL_PHASE} eq 'DESTRUCT';
  PKGDepDB::cmod::pkgdepdb_elf_delete($self->{ptr});
}

1;
