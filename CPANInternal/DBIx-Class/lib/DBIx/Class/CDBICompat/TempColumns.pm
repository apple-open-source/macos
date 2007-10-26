package # hide from PAUSE
    DBIx::Class::CDBICompat::TempColumns;

use strict;
use warnings;
use base qw/DBIx::Class/;

__PACKAGE__->mk_classdata('_temp_columns' => { });

sub _add_column_group {
  my ($class, $group, @cols) = @_;
  if ($group eq 'TEMP') {
    $class->_register_column_group($group => @cols);
    $class->mk_group_accessors('temp' => @cols);
    my %tmp = %{$class->_temp_columns};
    $tmp{$_} = 1 for @cols;
    $class->_temp_columns(\%tmp);
  } else {
    return $class->next::method($group, @cols);
  }
}

sub new {
  my ($class, $attrs, @rest) = @_;
  my %temp;
  foreach my $key (keys %$attrs) {
    $temp{$key} = delete $attrs->{$key} if $class->_temp_columns->{$key};
  }
  my $new = $class->next::method($attrs, @rest);
  foreach my $key (keys %temp) {
    $new->set_temp($key, $temp{$key});
  }
  return $new;
}


sub find_column {
  my ($class, $col, @rest) = @_;
  return $col if $class->_temp_columns->{$col};
  return $class->next::method($col, @rest);
}

sub get_temp {
  my ($self, $column) = @_;
  $self->throw_exception( "Can't fetch data as class method" ) unless ref $self;
  $self->throw_exception( "No such TEMP column '${column}'" ) unless $self->_temp_columns->{$column} ;
  return $self->{_temp_column_data}{$column}
    if exists $self->{_temp_column_data}{$column};
  return undef;
}

sub set_temp {
  my ($self, $column, $value) = @_;
  $self->throw_exception( "No such TEMP column '${column}'" )
    unless $self->_temp_columns->{$column};
  $self->throw_exception( "set_temp called for ${column} without value" )
    if @_ < 3;
  return $self->{_temp_column_data}{$column} = $value;
}

sub has_real_column {
  return 1 if shift->has_column(shift);
}

1;
