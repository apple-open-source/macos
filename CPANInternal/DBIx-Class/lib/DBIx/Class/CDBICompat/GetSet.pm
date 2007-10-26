package # hide from PAUSE
    DBIx::Class::CDBICompat::GetSet;

use strict;
use warnings;

#use base qw/Class::Accessor/;

sub get {
  my ($self, @cols) = @_;
  if (@cols > 1) {
    return map { $self->get_column($_) } @cols;
  } else {
    return $self->get_column($_[1]);
  }
}

sub set {
  return shift->set_column(@_);
}

1;
