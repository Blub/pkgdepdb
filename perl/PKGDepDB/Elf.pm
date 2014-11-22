package PKGDepDB::Elf;

use strict;
use warnings;
use PKGDepDB;
use PKGDepDB::cmod;
use PKGDepDB::CVal;

sub load($$) {
  my ($class, $file, $cfg) = @_;
  my $err = 0;
  my $ptr = PKGDepDB::cmod::pkgdepdb_elf_load($file, \$err, $cfg);

  if ($err or not $ptr) {
    return undef;
  }

  return $class->new($ptr);
}

sub class_string($) {
  my $self = shift;
  return PKGDepDB::cmod::pkgdepdb_elf_class_string($self->{ptr});
}

sub data_string($) {
  my $self = shift;
  return PKGDepDB::cmod::pkgdepdb_elf_data_string($self->{ptr});
}

sub osabi_string($) {
  my $self = shift;
  return PKGDepDB::cmod::pkgdepdb_elf_osabi_string($self->{ptr});
}

sub can_use($$$) {
  my ($sub, $obj, $strict) = @_;
  return PKGDepDB::cmod::pkgdepdb_elf_can_use($sub, $obj, $strict ? 1 : 0);
}

sub new {
  my ($class, $ptr) = @_;

  unless ($ptr) {
    $ptr = PKGDepDB::cmod::pkgdepdb_elf_new();
  }

  my $self = bless { ptr => $ptr }, $class;

  tie $self->{dirname}, 'PKGDepDB::CVal::Scalar', $ptr,
      \&PKGDepDB::cmod::pkgdepdb_elf_dirname,
      \&PKGDepDB::cmod::pkgdepdb_elf_set_dirname;
  tie $self->{basename}, 'PKGDepDB::CVal::Scalar', $ptr,
      \&PKGDepDB::cmod::pkgdepdb_elf_basename,
      \&PKGDepDB::cmod::pkgdepdb_elf_set_basename;
  tie $self->{ei_class}, 'PKGDepDB::CVal::Scalar', $ptr,
      \&PKGDepDB::cmod::pkgdepdb_elf_class,
      \&PKGDepDB::cmod::pkgdepdb_elf_set_class;
  tie $self->{ei_data}, 'PKGDepDB::CVal::Scalar', $ptr,
      \&PKGDepDB::cmod::pkgdepdb_elf_data,
      \&PKGDepDB::cmod::pkgdepdb_elf_set_data;
  tie $self->{ei_osabi}, 'PKGDepDB::CVal::Scalar', $ptr,
      \&PKGDepDB::cmod::pkgdepdb_elf_osabi,
      \&PKGDepDB::cmod::pkgdepdb_elf_set_osabi;
  tie $self->{rpath}, 'PKGDepDB::CVal::Scalar', $ptr,
      \&PKGDepDB::cmod::pkgdepdb_elf_rpath,
      \&PKGDepDB::cmod::pkgdepdb_elf_set_rpath;
  tie $self->{ei_runpath}, 'PKGDepDB::CVal::Scalar', $ptr,
      \&PKGDepDB::cmod::pkgdepdb_elf_runpath,
      \&PKGDepDB::cmod::pkgdepdb_elf_set_runpath;
  tie $self->{ei_interpreter}, 'PKGDepDB::CVal::Scalar', $ptr,
      \&PKGDepDB::cmod::pkgdepdb_elf_interpreter,
      \&PKGDepDB::cmod::pkgdepdb_elf_set_interpreter;
  tie @{$self->{needed}}, 'PKGDepDB::CVal::StringList', $ptr,
      \&PKGDepDB::cmod::pkgdepdb_elf_needed_count,
      \&PKGDepDB::cmod::pkgdepdb_elf_needed_get,
      \&PKGDepDB::cmod::pkgdepdb_elf_needed_set_i,
      \&PKGDepDB::cmod::pkgdepdb_elf_needed_del_i,
      \&PKGDepDB::cmod::pkgdepdb_elf_needed_add,
      \&PKGDepDB::cmod::pkgdepdb_elf_needed_insert_r,
      \&PKGDepDB::cmod::pkgdepdb_elf_needed_del_r,
      \&PKGDepDB::cmod::pkgdepdb_elf_needed_del_s,
      \&PKGDepDB::cmod::pkgdepdb_elf_needed_contains;
  tie @{$self->{missing}}, 'PKGDepDB::CVal::ROList', $ptr,
      \&PKGDepDB::cmod::pkgdepdb_elf_missing_count,
      \&PKGDepDB::cmod::pkgdepdb_elf_missing_get,
      \&PKGDepDB::cmod::pkgdepdb_elf_missing_contains;
  tie @{$self->{found}}, 'PKGDepDB::CVal::ROList', $ptr,
      \&PKGDepDB::cmod::pkgdepdb_elf_found_count,
      sub($$$$) {
        my ($ptr, $out, $index, $count) = @_;
        my @raw;
        $#raw = $#$out;
        PKGDepDB::cmod::pkgdepdb_elf_found_get($ptr, \@raw, $index, $count);
        $#{@$out} = 0;
        for my $elf (@raw) {
          push @$out, $class->new($elf);
        }
      },
      undef,
      sub($$) {
        my ($ptr, $key) = @_;
        if (my $elf = PKGDepDB::cmod::pkgdepdb_elf_found_get($ptr, $key)) {
          return $class->new($elf);
        }
        return undef;
      };

  return $self;
}

sub DESTROY {
  my $self = shift;
  return if ${^GLOBAL_PHASE} eq 'DESTRUCT';
  PKGDepDB::cmod::pkgdepdb_elf_unref($self->{ptr});
}

1;
