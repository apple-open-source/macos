use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

plan tests => 8;

my $schema = DBICTest->init_schema();

# Attempt sequential nested find_or_create with autoinc
# As a side effect re-test nested default create (both the main object and the relation are {})
my $bookmark_rs = $schema->resultset('Bookmark');
my $last_bookmark = $bookmark_rs->search ({}, { order_by => { -desc => 'id' }, rows => 1})->single;
my $last_link = $bookmark_rs->search_related ('link', {}, { order_by => { -desc => 'link.id' }, rows => 1})->single;

# find_or_create a bookmark-link combo with data for a non-existing link
my $o1 = $bookmark_rs->find_or_create ({ link => { url => 'something-weird' } });
is ($o1->id, $last_bookmark->id + 1, '1st bookmark ID');
is ($o1->link->id, $last_link->id + 1, '1st related link ID');

# find_or_create a bookmark-link combo without any data at all (default insert)
# should extend this test to all available Storage's, and fix them accordingly
my $o2 = $bookmark_rs->find_or_create ({ link => {} });
is ($o2->id, $last_bookmark->id + 2, '2nd bookmark ID');
is ($o2->link->id, $last_link->id + 2, '2nd related link ID');

# make sure the pre-existing link has only one related bookmark
is ($last_link->bookmarks->count, 1, 'Expecting only 1 bookmark and 1 link, someone mucked with the table!');

# find_or_create a bookmark withouyt any data, but supplying an existing link object
# should return $last_bookmark
my $o0 = $bookmark_rs->find_or_create ({ link => $last_link });
is_deeply ({ $o0->columns}, {$last_bookmark->columns}, 'Correctly identify a row given a relationship');

# inject an additional bookmark and repeat the test
# should warn and return the first row
my $o3 = $last_link->create_related ('bookmarks', {});
is ($o3->id, $last_bookmark->id + 3, '3rd bookmark ID');

local $SIG{__WARN__} = sub { warn @_ unless $_[0] =~ /Query returned more than one row/ };
my $oX = $bookmark_rs->find_or_create ({ link => $last_link });
is_deeply ({ $oX->columns}, {$last_bookmark->columns}, 'Correctly identify a row given a relationship');
