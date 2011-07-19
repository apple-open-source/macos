package # hide from PAUSE
    DBIx::Class::Relationship::HasOne;

use strict;
use warnings;
use Carp::Clan qw/^DBIx::Class/;

our %_pod_inherit_config = 
  (
   class_map => { 'DBIx::Class::Relationship::HasOne' => 'DBIx::Class::Relationship' }
  );

sub might_have {
  shift->_has_one('LEFT' => @_);
}

sub has_one {
  shift->_has_one(undef() => @_);
}

sub _has_one {
  my ($class, $join_type, $rel, $f_class, $cond, $attrs) = @_;
  unless (ref $cond) {
    $class->ensure_class_loaded($f_class);

    my $pri = $class->_get_primary_key;

    $class->throw_exception(
      "might_have/has_one needs a primary key  to infer a join; ".
      "${class} has none"
    ) if !defined $pri && (!defined $cond || !length $cond);

    my $f_class_loaded = eval { $f_class->columns };
    my ($f_key,$too_many,$guess);
    if (defined $cond && length $cond) {
      $f_key = $cond;
      $guess = "caller specified foreign key '$f_key'";
    } elsif ($f_class_loaded && $f_class->has_column($rel)) {
      $f_key = $rel;
      $guess = "using given relationship '$rel' for foreign key";
    } else {
      $f_key = $class->_get_primary_key($f_class);
      $guess = "using primary key of foreign class for foreign key";
    }
    $class->throw_exception(
      "No such column ${f_key} on foreign class ${f_class} ($guess)"
    ) if $f_class_loaded && !$f_class->has_column($f_key);
    $cond = { "foreign.${f_key}" => "self.${pri}" };
  }
  $class->_validate_cond($cond);
  $class->add_relationship($rel, $f_class,
   $cond,
   { accessor => 'single',
     cascade_update => 1, cascade_delete => 1,
     ($join_type ? ('join_type' => $join_type) : ()),
     %{$attrs || {}} });
  1;
}

sub _get_primary_key {
  my ( $class, $target_class ) = @_;
  $target_class ||= $class;
  my ($pri, $too_many) = eval { $target_class->_pri_cols };
  $class->throw_exception(
    "Can't infer join condition on ${target_class}: $@"
  ) if $@;

  $class->throw_exception(
    "might_have/has_one can only infer join for a single primary key; ".
    "${class} has more"
  ) if $too_many;
  return $pri;
}

sub _validate_cond {
  my ($class, $cond )  = @_;

  return if $ENV{DBIC_DONT_VALIDATE_RELS};
  return unless 'HASH' eq ref $cond;
  foreach my $foreign_id ( keys %$cond ) {
    my $self_id = $cond->{$foreign_id};

    # we can ignore a bad $self_id because add_relationship handles this
    # warning
    return unless $self_id =~ /^self\.(.*)$/;
    my $key = $1;
    my $column_info = $class->column_info($key);
    if ( $column_info->{is_nullable} ) {
      carp(qq'"might_have/has_one" must not be on columns with is_nullable set to true ($class/$key). This might indicate an incorrect use of those relationship helpers instead of belongs_to.');
    }
  }
}

1;
