#!/usr/bin/perl

use strict;
use warnings;
use PKGDepDB::Cfg;
use PKGDepDB::Db;

my $cfg = PKGDepDB::Cfg->new();
$cfg->load_default;
print $cfg->{database}, "\n";
print $cfg->database, "\n";

my $db = PKGDepDB::Db->new();
print join("; ", @{$db->{library_path}}), "\n";
push @{$db->{library_path}}, '/lib', '/usr/lib';
print join("; ", @{$db->{library_path}}), "\n";
print scalar(@{$db->{library_path}}), "\n";
print ${$db->{library_path}}[0], "\n";
print ${$db->{library_path}}[1], "\n";
${$db->{library_path}}[1] = "/usr/local/lib";
print join("; ", @{$db->{library_path}}), "\n";
${$db->{library_path}}[2] = '/usr/lib';
print join("; ", @{$db->{library_path}}), "\n";
${$db->{library_path}}[2] = '/usr/lib';
print join("; ", @{$db->{library_path}}), "\n";
${$db->{library_path}}[3] = '/usr/lib';
print join("; ", @{$db->{library_path}}), "\n";
delete ${$db->{library_path}}[0];
print join("; ", @{$db->{library_path}}), "\n";
