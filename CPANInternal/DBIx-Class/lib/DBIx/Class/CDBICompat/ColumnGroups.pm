package # hide from PAUSE
    DBIx::Class::CDBICompat::ColumnGroups;

use strict;
use warnings;

use base qw/DBIx::Class::Row/;

__PACKAGE__->mk_classdata('_column_groups' => { });

sub columns {
  my $proto = shift;
  my $class = ref $proto || $proto;
  my $group = shift || "All";
  $class->_add_column_group($group => @_) if @_;
  return $class->all_columns    if $group eq "All";
  return $class->primary_column if $group eq "Primary";
  return keys %{$class->_column_groups->{$group}};
}

sub _add_column_group {
  my ($class, $group, @cols) = @_;
  $class->add_columns(@cols);
  $class->_register_column_group($group => @cols);
}

sub _register_column_group {
  my ($class, $group, @cols) = @_;

  my $groups = { %{$class->_column_groups} };

  if ($group eq 'Primary') {
    $class->set_primary_key(@cols);
    $groups->{'Essential'}{$_} ||= {} for @cols;
  }

  if ($group eq 'All') {
    unless (exists $class->_column_groups->{'Primary'}) {
      $groups->{'Primary'}{$cols[0]} = {};
      $class->set_primary_key($cols[0]);
    }
    unless (exists $class->_column_groups->{'Essential'}) {
      $groups->{'Essential'}{$cols[0]} = {};
    }
  }

  $groups->{$group}{$_} ||= {} for @cols;

  $class->_column_groups($groups);
}

sub all_columns { return shift->result_source_instance->columns; }

sub primary_column {
  my ($class) = @_;
  my @pri = $class->primary_columns;
  return wantarray ? @pri : $pri[0];
}

sub find_column {
  my ($class, $col) = @_;
  return $col if $class->has_column($col);
}

sub __grouper {
  my ($class) = @_;
  my $grouper = { class => $class };
  return bless($grouper, 'DBIx::Class::CDBICompat::ColumnGroups::GrouperShim');
}

sub _find_columns {
  my ($class, @col) = @_;
  return map { $class->find_column($_) } @col;
}

package DBIx::Class::CDBICompat::ColumnGroups::GrouperShim;

sub groups_for {
  my ($self, @cols) = @_;
  my %groups;
  foreach my $col (@cols) {
    foreach my $group (keys %{$self->{class}->_column_groups}) {
      $groups{$group} = 1 if $self->{class}->_column_groups->{$group}->{$col};
    }
  }
  return keys %groups;
}
    

1;
