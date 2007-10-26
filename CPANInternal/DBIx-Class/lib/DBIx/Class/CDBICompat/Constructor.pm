package # hide from PAUSE
    DBIx::Class::CDBICompat::Constructor;

use strict;
use warnings;

sub add_constructor {
  my ($class, $meth, $sql) = @_;
  $class = ref $class if ref $class;
  no strict 'refs';
  *{"${class}::${meth}"} =
    sub {
      my ($class, @args) = @_;
      return $class->search_literal($sql, @args);
    };
}

1;
