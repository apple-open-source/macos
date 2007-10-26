package # hide from PAUSE
    DBIx::Class::CDBICompat::LazyLoading;

use strict;
use warnings;

sub resultset_instance {
  my $self = shift;
  my $rs = $self->next::method(@_);
  $rs = $rs->search(undef, { columns => [ $self->columns('Essential') ] });
  return $rs;
}

sub get_column {
  my ($self, $col) = @_;
  if ((ref $self) && (!exists $self->{'_column_data'}{$col})
    && $self->{'_in_storage'}) {
    $self->_flesh(grep { exists $self->_column_groups->{$_}{$col}
                           && $_ ne 'All' }
                   keys %{ $self->_column_groups || {} });
  }
  $self->next::method(@_[1..$#_]);
}

sub _ident_cond {
  my ($class) = @_;
  return join(" AND ", map { "$_ = ?" } $class->primary_columns);
}

sub _flesh {
  my ($self, @groups) = @_;
  @groups = ('All') unless @groups;
  my %want;
  $want{$_} = 1 for map { keys %{$self->_column_groups->{$_}} } @groups;
  if (my @want = grep { !exists $self->{'_column_data'}{$_} } keys %want) {
    my $cursor = $self->result_source->storage->select(
                $self->result_source->name, \@want,
                \$self->_ident_cond, { bind => [ $self->_ident_values ] });
    #my $sth = $self->storage->select($self->_table_name, \@want,
    #                                   $self->ident_condition);
    # Not sure why the first one works and this doesn't :(
    my @val = $cursor->next;
#warn "Flesh: ".join(', ', @want, '=>', @val);
    foreach my $w (@want) {
      $self->{'_column_data'}{$w} = shift @val;
    }
  }
}

1;
