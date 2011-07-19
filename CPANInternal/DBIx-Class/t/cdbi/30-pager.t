use strict;
use Test::More;

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  if ($@) {
    plan (skip_all => 'Class::Trigger and DBIx::ContextualFetch required');
    next;
  }
  eval "use DBD::SQLite";
  plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 6);
}

use lib 't/cdbi/testlib';
use Film;

my @film  = (
  Film->create({ Title => 'Film 1' }),
  Film->create({ Title => 'Film 2' }),
  Film->create({ Title => 'Film 3' }),
  Film->create({ Title => 'Film 4' }),
  Film->create({ Title => 'Film 5' }),
);

# first page
my ( $pager, $it ) = Film->page(
    {},
    { rows => 3,
      page => 1 }
);

is( $pager->entries_on_this_page, 3, "entries_on_this_page ok" );

is( $pager->next_page, 2, "next_page ok" );

is( $it->next->title, "Film 1", "iterator->next ok" );

$it->next;
$it->next;

is( $it->next, undef, "next past end of page ok" );

# second page
( $pager, $it ) = Film->page( 
    {},
    { rows => 3,
      page => 2 }
);

is( $pager->entries_on_this_page, 2, "entries on second page ok" );

is( $it->next->title, "Film 4", "second page first title ok" );
