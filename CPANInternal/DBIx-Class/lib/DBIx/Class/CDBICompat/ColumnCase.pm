package # hide from PAUSE
    DBIx::Class::CDBICompat::ColumnCase;

use strict;
use warnings;

use base qw/DBIx::Class/;

sub _register_column_group {
  my ($class, $group, @cols) = @_;
  return $class->next::method($group => map lc, @cols);
}

sub add_columns {
  my ($class, @cols) = @_;
  $class->mk_group_accessors(column => @cols);
  $class->result_source_instance->add_columns(map lc, @cols);
}

sub has_a {
  my ($class, $col, @rest) = @_;
  $class->next::method(lc($col), @rest);
  $class->mk_group_accessors('inflated_column' => $col);
  return 1;
}

sub has_many {
  my ($class, $rel, $f_class, $f_key, @rest) = @_;
  return $class->next::method($rel, $f_class, ( ref($f_key) ?
                                                          $f_key :
                                                          lc($f_key) ), @rest);
}

sub get_inflated_column {
  my ($class, $get, @rest) = @_;
  return $class->next::method(lc($get), @rest);
}

sub store_inflated_column {
  my ($class, $set, @rest) = @_;
  return $class->next::method(lc($set), @rest);
}

sub set_inflated_column {
  my ($class, $set, @rest) = @_;
  return $class->next::method(lc($set), @rest);
}

sub get_column {
  my ($class, $get, @rest) = @_;
  return $class->next::method(lc($get), @rest);
}

sub set_column {
  my ($class, $set, @rest) = @_;
  return $class->next::method(lc($set), @rest);
}

sub store_column {
  my ($class, $set, @rest) = @_;
  return $class->next::method(lc($set), @rest);
}

sub find_column {
  my ($class, $col) = @_;
  return $class->next::method(lc($col));
}

# _build_query
#
# Build a query hash for find, et al. Overrides Retrieve::_build_query.

sub _build_query {
  my ($self, $query) = @_;

  my %new_query;
  $new_query{lc $_} = $query->{$_} for keys %$query;

  return \%new_query;
}

sub _mk_group_accessors {
  my ($class, $type, $group, @fields) = @_;
  #warn join(', ', map { ref $_ ? (@$_) : ($_) } @fields);
  my @extra;
  foreach (@fields) {
    my ($acc, $field) = ref $_ ? @$_ : ($_, $_);
    #warn "$acc ".lc($acc)." $field";
    next if defined &{"${class}::${acc}"};
    push(@extra, [ lc $acc => $field ]);
  }
  return $class->next::method($type, $group,
                                                     @fields, @extra);
}

sub new {
  my ($class, $attrs, @rest) = @_;
  my %att;
  $att{lc $_} = $attrs->{$_} for keys %$attrs;
  return $class->next::method(\%att, @rest);
}

1;
