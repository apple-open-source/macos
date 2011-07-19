use strict;
use warnings;  

use Test::More qw(no_plan);
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

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

is( $it->pager->total_entries, 5, "total_entries ok" );

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

# test paging with chained searches
$it = $schema->resultset("CD")->search(
    {},
    { rows => 2,
      page => 2 }
)->search( undef, { order_by => 'title' } );

is( $it->count, 2, "chained searches paging ok" );

my $p = sub { $schema->resultset("CD")->page(1)->pager->entries_per_page; };

is($p->(), 10, 'default rows is 10');

$schema->default_resultset_attributes({ rows => 5 });

is($p->(), 5, 'default rows is 5');

# test page with offset
$it = $schema->resultset('CD')->search({}, {
    rows => 2,
    page => 2,
    offset => 1,
    order_by => 'cdid'
});

my $row = $schema->resultset('CD')->search({}, {
    order_by => 'cdid', 
    offset => 3,
    rows => 1
})->single;

is($row->cdid, $it->first->cdid, 'page with offset');
