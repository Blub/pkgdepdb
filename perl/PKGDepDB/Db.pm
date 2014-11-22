use strict;
use warnings;
use PKGDepDB;
use PKGDepDB::cmod;
use PKGDepDB::CVal;

package PKGDepDB::Db;

sub DESTROY {
  my $self = shift;
  return if ${^GLOBAL_PHASE} eq 'DESTRUCT';
  PKGDepDB::cmod::pkgdepdb_db_delete($self->{ptr});
}

sub load($$) {
  my ($self, $file) = @_;
  my $ptr = $self->{ptr};
  if (!PKGDepDB::cmod::pkgdepdb_db_load($ptr, $file)) {
    $self->{error} = cmod::error();
    return 0;
  }
  $self->{loaded_version} = PKGDepDB::cmod::pkgdepdb_db_loaded_version($ptr);
  return 1;
}

sub store($$) {
  my ($self, $file) = @_;
  if (!PKGDepDB::cmod::pkgdepdb_db_store($self->{ptr}, $file)) {
    $self->{error} = PKGDepDB::cmod::error();
    return 0;
  }
  return 1;
}

sub relink_all($) {
  my $self = shift;
  PKGDepDB::cmod::pkgdepdb_db_relink_all($self->{ptr});
}

sub fix_paths($) {
  my $self = shift;
  PKGDepDB::cmod::pkgdepdb_db_fix_paths($self->{ptr});
}

sub wipe_packages($) {
  my $self = shift;
  PKGDepDB::cmod::pkgdepdb_db_wipe_packages($self->{ptr});
}

sub wipe_filelists($) {
  my $self = shift;
  PKGDepDB::cmod::pkgdepdb_db_wipe_filelists($self->{ptr});
}

#pkgdepdb_bool pkgdepdb_db_package_is_broken(pkgdepdb_db*, pkgdepdb_pkg*);
#pkgdepdb_bool pkgdepdb_db_object_is_broken(pkgdepdb_db*, pkgdepdb_elf);

sub new($) {
  my $class = shift;
  my $cfg = shift;
  my $ptr = PKGDepDB::cmod::pkgdepdb_db_new($cfg);
  my $self = bless { 'ptr' => $ptr }, $class;
  tie $self->{strict_linking}, 'PKGDepDB::CVal::Scalar', $ptr,
    \&PKGDepDB::cmod::pkgdepdb_db_strict_linking,
    \&PKGDepDB::cmod::pkgdepdb_db_set_strict_linking;
  tie $self->{name}, 'PKGDepDB::CVal::Scalar', $ptr,
    \&PKGDepDB::cmod::pkgdepdb_db_name,
    \&PKGDepDB::cmod::pkgdepdb_db_set_name;
  tie @{$self->{library_path}}, 'PKGDepDB::CVal::StringList', $ptr,
    \&PKGDepDB::cmod::pkgdepdb_db_library_path_count,
    \&PKGDepDB::cmod::pkgdepdb_db_library_path_get,
    \&PKGDepDB::cmod::pkgdepdb_db_library_path_set_i,
    \&PKGDepDB::cmod::pkgdepdb_db_library_path_del_i,
    \&PKGDepDB::cmod::pkgdepdb_db_library_path_add,
    \&PKGDepDB::cmod::pkgdepdb_db_library_path_insert_r,
    \&PKGDepDB::cmod::pkgdepdb_db_library_path_del_r,
    \&PKGDepDB::cmod::pkgdepdb_db_library_path_del_s,
    \&PKGDepDB::cmod::pkgdepdb_db_library_path_contains;
  tie @{$self->{ignored_files}}, 'PKGDepDB::CVal::StringSet', $ptr,
    \&PKGDepDB::cmod::pkgdepdb_db_ignored_files_count,
    \&PKGDepDB::cmod::pkgdepdb_db_ignored_files_get,
    \&PKGDepDB::cmod::pkgdepdb_db_ignored_files_del_i,
    \&PKGDepDB::cmod::pkgdepdb_db_ignored_files_add_r,
    \&PKGDepDB::cmod::pkgdepdb_db_ignored_files_add,
    \&PKGDepDB::cmod::pkgdepdb_db_ignored_files_del_r,
    \&PKGDepDB::cmod::pkgdepdb_db_ignored_files_del_s;
  tie @{$self->{base_packages}}, 'PKGDepDB::CVal::StringSet', $ptr,
    \&PKGDepDB::cmod::pkgdepdb_db_base_packages_count,
    \&PKGDepDB::cmod::pkgdepdb_db_base_packages_get,
    \&PKGDepDB::cmod::pkgdepdb_db_base_packages_del_i,
    \&PKGDepDB::cmod::pkgdepdb_db_base_packages_add_r,
    \&PKGDepDB::cmod::pkgdepdb_db_base_packages_add,
    \&PKGDepDB::cmod::pkgdepdb_db_base_packages_del_r,
    \&PKGDepDB::cmod::pkgdepdb_db_base_packages_del_s;
  tie @{$self->{assume_found}}, 'PKGDepDB::CVal::StringSet', $ptr,
    \&PKGDepDB::cmod::pkgdepdb_db_assume_found_count,
    \&PKGDepDB::cmod::pkgdepdb_db_assume_found_get,
    \&PKGDepDB::cmod::pkgdepdb_db_assume_found_del_i,
    \&PKGDepDB::cmod::pkgdepdb_db_assume_found_add_r,
    \&PKGDepDB::cmod::pkgdepdb_db_assume_found_add,
    \&PKGDepDB::cmod::pkgdepdb_db_assume_found_del_r,
    \&PKGDepDB::cmod::pkgdepdb_db_assume_found_del_s;

  tie @{$self->{packages}}, 'PKGDepDB::Db::PackageList', $ptr;

  return $self;
}

package PKGDepDB::Db::PackageList;
use Carp qw(carp);
sub PACKAGE { return 'PKGDepDB::Db::PackageList'; }

sub TIEARRAY {
  my ($class, $ptr) = @_;

  return bless { ptr => $ptr }, $class;
}

sub FETCH {
  my ($self, $index) = @_;
  my $ptr = $self->{ptr};
  my @out = (0);
  PKGDepDB::cmod::pkgdepdb_db_package_get($ptr, \@out, $index, 1);
  return PKGDepDB::Package->new($out[0], 1);
}

sub FETCHSIZE {
  my $self = shift;
  return PKGDepDB::cmod::pkgdepdb_db_package_count($self->{ptr});
}

sub STORE {
  carp("STORE not allowed on packages array");
}

sub CLEAR {
  carp("CLEAR not allowed on packages array, use wipe_packages on the Db");
}

sub EXISTS {
  my ($self, $key) = @_;
  if ("$key" =~ /^[+-]?\d+$/) {
    return $key < $self->FETCHSIZE();
  }
  return PKGDepDB::cmod::pkgdepdb_db_package_find($self->{ptr}, $key) ? 1 : 0;
}

sub DELETE {
  carp("DELETE not allowed on packages array, use Db's delete/remove method");
}

sub PUSH {
  my $self = shift;
  my $ptr = $self->{ptr};
  for my $pkg (@_) {
    PKGDepDB::cmod::pkgdepdb_db_package_install($ptr, $pkg);
  }
}

sub POP {
  carp("POP not allowed on packages array, use Db's delete/remove method");
}

sub SHIFT {
  carp("UNSHIFT not allowed on packages array, use Db's delete/remove method");
}

sub UNSHIFT {
  carp("UNSHIFT not allowed on packages array, use push / Db::install");
}

sub SPLICE {
  carp("SPLICE not allowed on packages array");
}

package PKGDepDB::Db::ObjectList;
use Carp qw(carp);
sub PACKAGE { return 'PKGDepDB::Db::PackageList'; }

sub TIEARRAY {
  my ($class, $ptr) = @_;

  return bless { ptr => $ptr }, $class;
}

sub FETCH {
  my ($self, $index) = @_;
  my $ptr = $self->{ptr};
  my @out = (0);
  PKGDepDB::cmod::pkgdepdb_db_object_get($ptr, \@out, $index, 1);
  return PKGDepDB::Elf->new($out[0], 1);
}

sub FETCHSIZE {
  my $self = shift;
  return PKGDepDB::cmod::pkgdepdb_db_object_count($self->{ptr});
}

sub STORE   { carp("Db's object array is read-only"); }
sub CLEAR   { carp("Db's object array is read-only"); }
sub DELETE  { carp("Db's object array is read-only"); }
sub PUSH    { carp("Db's object array is read-only"); }
sub POP     { carp("Db's object array is read-only"); }
sub SHIFT   { carp("Db's object array is read-only"); }
sub UNSHIFT { carp("Db's object array is read-only"); }
sub SPLICE  { carp("Db's object array is read-only"); }


1;
