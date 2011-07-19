use strict;
use warnings;

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 2;

my $bookmark = $schema->resultset("Bookmark")->find(1);
my $link = $bookmark->link;
my $link_id = $link->id;

my $new_link = $schema->resultset("Link")->new({
    id      => 42,
    url     => "http://monstersarereal.com",
    title   => "monstersarereal.com"
});

# Changing a relationship by id rather than by object would cause
# old related_resultsets to be used.
$bookmark->link($new_link->id);
is $bookmark->link->id, $new_link->id;

$bookmark->update;
is $bookmark->link->id, $new_link->id;
