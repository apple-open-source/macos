use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 12;

# first page
my $it = $schema->resultset("CD")->search(
    {},
    { order_by => 'title',
      rows => 3,
      page => 1 }
);

is( $it->pager->entries_on_this_page, 3, "entries_on_this_page ok" );

is( $it->pager->next_page, 2, "next_page ok" );

is( $it->count, 3, "count on paged rs ok" );

is( $it->next->title, "Caterwaulin' Blues", "iterator->next ok" );

$it->next;
$it->next;

is( $it->next, undef, "next past end of page ok" );

# second page, testing with array
my @page2 = $schema->resultset("CD")->search( 
    {},
    { order_by => 'title',
      rows => 3,
      page => 2 }
);

is( $page2[0]->title, "Generic Manufactured Singles", "second page first title ok" );

# page a standard resultset
$it = $schema->resultset("CD")->search(
  {},
  { order_by => 'title',
    rows => 3 }
);
my $page = $it->page(2);

is( $page->count, 2, "standard resultset paged rs count ok" );

is( $page->next->title, "Generic Manufactured Singles", "second page of standard resultset ok" );

# test software-based limit paging
$it = $schema->resultset("CD")->search(
  {},
  { order_by => 'title',
    rows => 3,
    page => 2,
    software_limit => 1 }
);
is( $it->pager->entries_on_this_page, 2, "software entries_on_this_page ok" );

is( $it->pager->previous_page, 1, "software previous_page ok" );

is( $it->count, 2, "software count on paged rs ok" );

is( $it->next->title, "Generic Manufactured Singles", "software iterator->next ok" );

