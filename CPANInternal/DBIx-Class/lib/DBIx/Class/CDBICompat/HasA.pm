package # hide from PAUSE
    DBIx::Class::CDBICompat::HasA;

use strict;
use warnings;

sub has_a {
  my ($self, $col, $f_class, %args) = @_;
  $self->throw_exception( "No such column ${col}" ) unless $self->has_column($col);
  $self->ensure_class_loaded($f_class);
  if ($args{'inflate'} || $args{'deflate'}) { # Non-database has_a
    if (!ref $args{'inflate'}) {
      my $meth = $args{'inflate'};
      $args{'inflate'} = sub { $f_class->$meth(shift); };
    }
    if (!ref $args{'deflate'}) {
      my $meth = $args{'deflate'};
      $args{'deflate'} = sub { shift->$meth; };
    }
    $self->inflate_column($col, \%args);
    return 1;
  }

  $self->belongs_to($col, $f_class);
  return 1;
}

sub search {
  my $self = shift;
  my $attrs = {};
  if (@_ > 1 && ref $_[$#_] eq 'HASH') {
    $attrs = { %{ pop(@_) } };
  }
  my $where = (@_ ? ((@_ == 1) ? ((ref $_[0] eq "HASH") ? { %{+shift} } : shift)
                               : {@_})
                  : undef());
  if (ref $where eq 'HASH') {
    foreach my $key (keys %$where) { # has_a deflation hack
      $where->{$key} = ''.$where->{$key}
        if eval { $where->{$key}->isa('DBIx::Class') };
    }
  }
  $self->next::method($where, $attrs);
}

1;
