package # hide from PAUSE Indexer
    DBIx::Class::CDBICompat::AccessorMapping;

use strict;
use warnings;

sub mk_group_accessors {
  my ($class, $group, @cols) = @_;
  unless ($class->can('accessor_name') || $class->can('mutator_name')) {
    return $class->next::method($group => @cols);
  }
  foreach my $col (@cols) {
    my $ro_meth = ($class->can('accessor_name')
                    ? $class->accessor_name($col)
                    : $col);
    my $wo_meth = ($class->can('mutator_name')
                    ? $class->mutator_name($col)
                    : $col);
    #warn "$col $ro_meth $wo_meth";
    if ($ro_meth eq $wo_meth) {
      $class->next::method($group => [ $ro_meth => $col ]);
    } else {
      $class->mk_group_ro_accessors($group => [ $ro_meth => $col ]);
      $class->mk_group_wo_accessors($group => [ $wo_meth => $col ]);
    }
  }
}

sub new {
  my ($class, $attrs, @rest) = @_;
  $class->throw_exception( "create needs a hashref" ) unless ref $attrs eq 'HASH';
  foreach my $col ($class->columns) {
    if ($class->can('accessor_name')) {
      my $acc = $class->accessor_name($col);
      $attrs->{$col} = delete $attrs->{$acc} if exists $attrs->{$acc};
    }
    if ($class->can('mutator_name')) {
      my $mut = $class->mutator_name($col);
      $attrs->{$col} = delete $attrs->{$mut} if exists $attrs->{$mut};
    }
  }
  return $class->next::method($attrs, @rest);
}

1;
