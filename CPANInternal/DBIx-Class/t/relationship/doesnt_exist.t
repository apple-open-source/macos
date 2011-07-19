use strict;
use warnings;

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 3;

my $bookmark = $schema->resultset("Bookmark")->find(1);
my $link = $bookmark->link;
my $link_id = $link->id;
ok $link->id;

$link->delete;
is $schema->resultset("Link")->search(id => $link_id)->count, 0,
    "link $link_id was deleted";

# Get a fresh object with nothing cached
$bookmark = $schema->resultset("Bookmark")->find($bookmark->id);

# This would create a new link row if none existed
$bookmark->link;

is $schema->resultset("Link")->search(id => $link_id)->count, 0,
    'accessor did not create a link object where there was none';
