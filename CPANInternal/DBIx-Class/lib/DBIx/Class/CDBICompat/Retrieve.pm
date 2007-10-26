package # hide from PAUSE
    DBIx::Class::CDBICompat::Retrieve;

use strict;
use warnings FATAL => 'all';


sub retrieve {
  my $self = shift;
  die "No args to retrieve" unless @_ > 0;

  my @cols = $self->primary_columns;

  my $query;
  if (ref $_[0] eq 'HASH') {
    $query = { %{$_[0]} };
  }
  elsif (@_ == @cols) {
    $query = {};
    @{$query}{@cols} = @_;
  }
  else {
    $query = {@_};
  }

  $query = $self->_build_query($query);
  $self->find($query);
}

sub find_or_create {
  my $self = shift;
  my $query = ref $_[0] eq 'HASH' ? shift : {@_};

  $query = $self->_build_query($query);
  $self->next::method($query);
}

# _build_query
#
# Build a query hash. Defaults to a no-op; ColumnCase overrides.

sub _build_query {
  my ($self, $query) = @_;

  return $query;
}

sub retrieve_from_sql {
  my ($class, $cond, @rest) = @_;
  $cond =~ s/^\s*WHERE//i;
  $class->search_literal($cond, @rest);
}

sub retrieve_all      { shift->search              }
sub count_all         { shift->count               }
  # Contributed by Numa. No test for this though.

1;
