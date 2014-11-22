package PKGDepDB::Cfg;

use base qw(Exporter);
our @EXPORT_OK = qw(LOG_LEVEL_DEBUG LOG_LEVEL_MESSAGE LOG_LEVEL_PRINT
                    LOG_LEVEL_WARN LOG_LEVEL_ERROR
                    JSONBITS_QUERY JSONBITS_DB
                   );

use strict;
use warnings;
use PKGDepDB;
use PKGDepDB::cmod;
use PKGDepDB::CVal;

sub load($$) {
  my ($self, $file) = @_;
  if (!PKGDepDB::cmod::pkgdepdb_cfg_load($self->{ptr}, $file)) {
    $self->{error} = PKGDepDB::cmod::error();
    return 0;
  }
  return 1;
}

sub load_default($) {
  my ($self) = @_;
  if (!PKGDepDB::cmod::pkgdepdb_cfg_load_default($self->{ptr})) {
    $self->{error} = PKGDepDB::cmod::error();
    return 0;
  }
  return 1;
}

sub read($$$) {
  my ($self, $name, $data) = @_;
  if (!PKGDepDB::cmod::pkgdepdb_cfg_read($self->{ptr}, $name, $data, length($data)))
  {
    $self->{error} = PKGDepDB::cmod::error();
    return 0;
  }
  return 1;
}

sub database($) {
  my $self = shift;
  return PKGDepDB::cmod::pkgdepdb_cfg_database($self->{ptr});
}

sub verbosity($) {
  my $self = shift;
  return PKGDepDB::cmod::pkgdepdb_cfg_verbosity($self->{ptr});
}

sub quiet($) {
  my $self = shift;
  return PKGDepDB::cmod::pkgdepdb_cfg_quiet($self->{ptr});
}

sub package_depends($) {
  my $self = shift;
  return PKGDepDB::cmod::pkgdepdb_cfg_package_depends($self->{ptr});
}

sub package_file_lists($) {
  my $self = shift;
  return PKGDepDB::cmod::pkgdepdb_cfg_package_file_lists($self->{ptr});
}

sub package_info($) {
  my $self = shift;
  return PKGDepDB::cmod::pkgdepdb_cfg_package_info($self->{ptr});
}

sub max_jobs($) {
  my $self = shift;
  return PKGDepDB::cmod::pkgdepdb_cfg_max_jobs($self->{ptr});
}

use constant LOG_LEVEL_DEBUG   => $PKGDepDB::cmod::PKGDEPDB_CFG_LOG_LEVEL_DEBUG;
use constant LOG_LEVEL_MESSAGE => $PKGDepDB::cmod::PKGDEPDB_CFG_LOG_LEVEL_MESSAGE;
use constant LOG_LEVEL_PRINT   => $PKGDepDB::cmod::PKGDEPDB_CFG_LOG_LEVEL_PRINT;
use constant LOG_LEVEL_WARN    => $PKGDepDB::cmod::PKGDEPDB_CFG_LOG_LEVEL_WARN;
use constant LOG_LEVEL_ERROR   => $PKGDepDB::cmod::PKGDEPDB_CFG_LOG_LEVEL_ERROR;

sub log_level($) {
  my $self = shift;
  return PKGDepDB::cmod::pkgdepdb_cfg_log_level($self->{ptr});
}

use constant JSONBITS_QUERY => $PKGDepDB::cmod::PKGDEPDB_JSONBITS_QUERY;
use constant JSONBITS_DB    => $PKGDepDB::cmod::PKGDEPDB_JSONBITS_DB;

sub json($) {
  my $self = shift;
  return PKGDepDB::cmod::pkgdepdb_cfg_json($self->{ptr});
}

sub new {
  my $class = shift;
  my $self = bless { 'ptr' => PKGDepDB::cmod::pkgdepdb_cfg_new() }, $class;
  tie $self->{database}, 'PKGDepDB::CVal::Scalar', $self->{ptr},
      \&PKGDepDB::cmod::pkgdepdb_cfg_database,
      \&PKGDepDB::cmod::pkgdepdb_cfg_set_database;
  tie $self->{verbosity}, 'PKGDepDB::CVal::Scalar', $self->{ptr},
      \&PKGDepDB::cmod::pkgdepdb_cfg_verbosity,
      \&PKGDepDB::cmod::pkgdepdb_cfg_set_verbosity;
  tie $self->{quiet}, 'PKGDepDB::CVal::Scalar', $self->{ptr},
      \&PKGDepDB::cmod::pkgdepdb_cfg_quiet,
      \&PKGDepDB::cmod::pkgdepdb_cfg_set_quiet;
  tie $self->{package_depends}, 'PKGDepDB::CVal::Scalar', $self->{ptr},
      \&PKGDepDB::cmod::pkgdepdb_cfg_package_depends,
      \&PKGDepDB::cmod::pkgdepdb_cfg_set_package_depends;
  tie $self->{package_file_lists}, 'PKGDepDB::CVal::Scalar', $self->{ptr},
      \&PKGDepDB::cmod::pkgdepdb_cfg_package_file_lists,
      \&PKGDepDB::cmod::pkgdepdb_cfg_set_package_file_lists;
  tie $self->{package_info}, 'PKGDepDB::CVal::Scalar', $self->{ptr},
      \&PKGDepDB::cmod::pkgdepdb_cfg_package_info,
      \&PKGDepDB::cmod::pkgdepdb_cfg_set_package_info;
  tie $self->{max_jobs}, 'PKGDepDB::CVal::Scalar', $self->{ptr},
      \&PKGDepDB::cmod::pkgdepdb_cfg_max_jobs,
      \&PKGDepDB::cmod::pkgdepdb_cfg_set_max_jobs;
  tie $self->{log_level}, 'PKGDepDB::CVal::Scalar', $self->{ptr},
      \&PKGDepDB::cmod::pkgdepdb_cfg_log_level,
      \&PKGDepDB::cmod::pkgdepdb_cfg_set_log_level;
  tie $self->{json}, 'PKGDepDB::CVal::Scalar', $self->{ptr},
      \&PKGDepDB::cmod::pkgdepdb_cfg_json,
      \&PKGDepDB::cmod::pkgdepdb_cfg_set_json;
  return $self;
}

sub DESTROY {
  my $self = shift;
  return if ${^GLOBAL_PHASE} eq 'DESTRUCT';
  PKGDepDB::cmod::pkgdepdb_cfg_delete($self->{ptr});
}

1;
