package # hide from PAUSE
    DBIx::Class::CDBICompat::Pager;

use strict;
use warnings FATAL => 'all';

*pager = \&page;

sub page {
  my $class = shift;

  my $rs = $class->search(@_);
  unless ($rs->{attrs}{page}) {
    $rs = $rs->page(1);
  }
  return ( $rs->pager, $rs );
}

1;
