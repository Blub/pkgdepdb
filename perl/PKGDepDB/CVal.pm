use strict;
use warnings;

package PKGDepDB::CVal::Scalar;
use Carp qw(carp);

sub TIESCALAR {
  my ($class, $ptr, $get, $set) = @_;

  return bless [$ptr, $get, $set], $class;
}

sub FETCH {
  my $self = shift;
  carp("wrong type") unless 2 == $#$self;
  return $self->[1]->($self->[0]);
}

sub STORE {
  my $self = shift;
  carp("wrong type") unless 2 == $#$self;
  carp("attempt to alter a read only value") unless defined($self->[2]);
  my $value = shift;
  $self->[2]->($self->[0], $value);
}

package PKGDepDB::CVal::StringList;
use Carp qw(carp);

# __PACKAGE__ is 5.20.1+
sub PACKAGE { return 'PKGDepDB::CVal::StringList' }

sub TIEARRAY {
  my ($class, $ptr,
      $count, $get, $set, $del,
      $push, $insert, $del_r, $del_s, $contains) = @_;
  return bless {
    ptr      => $ptr,
    count    => $count,
    get      => $get,
    set      => $set,
    del      => $del,
    push     => $push,
    insert   => $insert,
    del_r    => $del_r,
    del_s    => $del_s,
    contains => $contains }, $class;
}

sub FETCH {
  my ($self, $index) = @_;
  my $ptr = $self->{ptr};
  my @out = (0);
  $self->{get}->($ptr, \@out, $index, 1);
  return shift @out;
}

sub STORE {
  my ($self, $index, $value) = @_;
  my $ptr = $self->{ptr};
  my $count = $self->{count}->($ptr);

  if ($index == $count) {
    $self->{push}->($ptr, $value);
    #$self->{insert}->($ptr, $self->{count}->($ptr), 1, [$value]);
    return;
  }
  carp("out of bounds index") if $index > $count;

  my $set = $self->{set};
  carp(PACKAGE . "::STORE: no usable set function available.") unless $set;

  $set->($ptr, $index, $value);
}

sub FETCHSIZE {
  my $self = shift;
  return $self->{count}->($self->{ptr});
}

sub CLEAR {
  my $self = shift;
  my $ptr = $self->{ptr};
  if (defined(my $del = $self->{del_r})) {
    my $count = $self->{count}->($ptr);
    $del->($ptr, 0, $count);
    return;
  }
  if (defined (my $del = $self->{del})) {
    my $count = $self->{count}->($ptr);
    return unless $count;
    for my $i (1..$count) {
      $del->($ptr, 0);
    }
    return;
  }
  carp(PACKAGE . "::CLEAR: no usable delete function available.");
}

sub EXISTS {
  my ($self, $key) = @_;
  if ("$key" =~ /^[+-]?\d+$/) {
    return $key < $self->FETCHSIZE();
  }
  if (my $contains = $self->{contains}) {
    return $contains->($self->{ptr}, $key);
  }
  return 0;
}

sub DELETE {
  my ($self, $key) = @_;
  my $ptr = $self->{ptr};
  if ("$key" =~ /^[+-]?\d+$/) {
    my $count = $self->{count}($ptr);
    if (my $del = $self->{del}) {
      $del->($ptr, $key);
    } else {
      carp(PACKAGE . "::DELETE: no simple delete function available.");
    }
    return;
  }
  my $del = $self->{del_s};
  carp(PACKAGE . "::DELETE: no usable delete function available.");
  $del->($ptr, $key);
}

sub PUSH {
  my $self = shift;
  my $ptr = $self->{ptr};
  if (my $insert = $self->{insert}) {
    $insert->($ptr, $self->{count}->($ptr), scalar(@_), \@_);
    return;
  }
  my $push = $self->{push};
  carp(PACKAGE . "::PUSH: no usable push function available.") unless $push;
  for my $elem (@_) {
    $push->($ptr, $elem);
  }
}

sub POP {
  my $self = shift;
  my $ptr = $self->{ptr};
  my $count = $self->{count}->($ptr);
  return undef unless $count > 0;
  my @out = (0);
  $self->{get}->($ptr, \@out, $count-1, 1);
  $self->{del}->($count-1);
  return shift @out;
}

sub SHIFT {
  my $self = shift;
  my $ptr = $self->{ptr};
  my $count = $self->{count}->($ptr);
  return undef unless $count > 0;
  my @out = (0);
  $self->{get}->($ptr, \@out, 0, 1);
  $self->{del}->($count-1);
  return shift @out,
}

sub UNSHIFT {
  my $self = shift;
  my $ptr = $self->{ptr};
  my $insert = $self->{insert};
  carp(PACKAGE . "::UNSHIFT: no insert function available.") unless $insert;
  $insert->($ptr, \@_);
}

sub SPLICE {
  my $self = shift;
  my $offset = shift;
  my $length = shift;
  my $ptr = $self->{ptr};
  $self->{del_r}->($ptr, $offset, $length) if $length > 0;
  if (@_) {
    $self->{insert}->($ptr, $offset, \@_);
  }
}

package PKGDepDB::CVal::StringSet;
use Carp qw(carp);
sub PACKAGE { return 'PKGDepDB::CVal::StringSet' }

sub TIEARRAY {
  my ($class, $ptr, $count, $get, $del, $add, $push,
      $del_r, $del_s, $contains) = @_;
  return bless {
    ptr      => $ptr,
    count    => $count,
    get      => $get,
    del      => $del,
    add      => $add,
    push     => $push,
    del_r    => $del_r,
    del_s    => $del_s,
    contains => $contains }, $class;
}

sub FETCH {
  my ($self, $index) = @_;
  my $ptr = $self->{ptr};
  my @out = (0);
  $self->{get}->($ptr, \@out, $index, 1);
  return shift @out;
}

sub FETCHSIZE {
  my $self = shift;
  return $self->{count}->($self->{ptr});
}

sub CLEAR {
  my $self = shift;
  my $ptr = $self->{ptr};
  if (defined(my $del = $self->{del_r})) {
    my $count = $self->{count}->($ptr);
    $del->($ptr, 0, $count);
    return;
  }
  if (defined (my $del = $self->{del})) {
    my $count = $self->{count}->($ptr);
    return unless $count;
    for my $i (1..$count) {
      $del->($ptr, 0);
    }
    return;
  }
  carp(PACKAGE . "::CLEAR: no usable delete function available.");
}

sub EXISTS {
  my ($self, $key) = @_;
  return $self->{contains}->($self->{ptr}, $key);
}

sub DELETE {
  my ($self, $key) = @_;
  my $ptr = $self->{ptr};
  if ("$key" =~ /^[+-]?\d+$/) {
    my $count = $self->{count}($ptr);
    if (my $del = $self->{del}) {
      $del->($ptr, $key);
    } else {
      carp(PACKAGE . "::DELETE: no simple delete function available.");
    }
    return;
  }
  my $del = $self->{del_s};
  carp(PACKAGE . "::DELETE: no usable delete function available.");
  $del->($ptr, $key);
}

sub PUSH {
  my $self = shift;
  my $ptr = $self->{ptr};
  $self->{add}->($ptr, scalar(@_), \@_);
}

sub POP {
  my $self = shift;
  my $ptr = $self->{ptr};
  my $count = $self->{count}->($ptr);
  return undef unless $count > 0;
  my @out = (0);
  $self->{get}->($ptr, \@out, $count-1, 1);
  $self->{del}->($count-1);
  return shift @out;
}

sub SHIFT {
  my $self = shift;
  my $ptr = $self->{ptr};
  my $count = $self->{count}->($ptr);
  return undef unless $count > 0;
  my @out = (0);
  $self->{get}->($ptr, \@out, 0, 1);
  $self->{del}->($count-1);
  return shift @out;
}

sub UNSHIFT {
  my $self = shift;
  my $ptr = $self->{ptr};
  $self->{add}->($ptr, scalar(@_), \@_);
}

sub SPLICE {
  my $self = shift;
  my $offset = shift;
  my $length = shift;
  my $ptr = $self->{ptr};
  $self->{del_r}->($ptr, $offset, $length) if $length > 0;
  if (@_) {
    $self->{add}->($ptr, \@_);
  }
}

1;
