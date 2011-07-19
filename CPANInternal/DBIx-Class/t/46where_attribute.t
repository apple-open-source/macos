use strict;
use warnings;

use Test::More;
use lib qw(t/lib);
use DBICTest;
my $schema = DBICTest->init_schema();

plan tests => 19;

# select from a class with resultset_attributes
my $resultset = $schema->resultset('BooksInLibrary');
is($resultset, 3, "select from a class with resultset_attributes okay");

# now test out selects through a resultset
my $owner = $schema->resultset('Owners')->find({name => "Newton"});
my $programming_perl = $owner->books->find_or_create({ title => "Programming Perl" });
is($programming_perl->id, 1, 'select from a resultset with find_or_create for existing entry ok');

# and inserts?
my $see_spot;
$see_spot = eval { $owner->books->find_or_create({ title => "See Spot Run" }) };
if ($@) { print $@ }
ok(!$@, 'find_or_create on resultset with attribute for non-existent entry did not throw');
ok(defined $see_spot, 'successfully did insert on resultset with attribute for non-existent entry');

my $see_spot_rs = $owner->books->search({ title => "See Spot Run" });
eval { $see_spot_rs->delete(); };
if ($@) { print $@ }
ok(!$@, 'delete on resultset with attribute did not throw');
is($see_spot_rs->count(), 0, 'delete on resultset with attributes succeeded');

# many_to_many tests
my $collection = $schema->resultset('Collection')->search({collectionid => 1});
my $pointy_objects = $collection->search_related('collection_object')->search_related('object', { type => "pointy"});
my $pointy_count = $pointy_objects->count();
is($pointy_count, 2, 'many_to_many explicit query through linking table with query starting from resultset count correct');

$collection = $schema->resultset('Collection')->find(1);
$pointy_objects = $collection->search_related('collection_object')->search_related('object', { type => "pointy"});
$pointy_count = $pointy_objects->count();
is($pointy_count, 2, 'many_to_many explicit query through linking table with query starting from row count correct');

# use where on many_to_many query
$collection = $schema->resultset('Collection')->find(1);
$pointy_objects = $collection->search_related('collection_object')->search_related('object', {}, { where => { 'object.type' => 'pointy' } });
is($pointy_objects->count(), 2, 'many_to_many explicit query through linking table with where starting from row count correct');

$collection = $schema->resultset('Collection')->find(1);
$pointy_objects = $collection->pointy_objects();
$pointy_count = $pointy_objects->count();
is($pointy_count, 2, 'many_to_many resultset with where in resultset attrs count correct');

# add_to_$rel on many_to_many with where containing a required field
eval {$collection->add_to_pointy_objects({ value => "Nail" }) };
if ($@) { print $@ }
ok( !$@, 'many_to_many add_to_$rel($hash) with where in relationship attrs did not throw');
is($pointy_objects->count, $pointy_count+1, 'many_to_many add_to_$rel($hash) with where in relationship attrs count correct');
$pointy_count = $pointy_objects->count();

my $pen = $schema->resultset('TypedObject')->create({ value => "Pen", type => "pointy"});
eval {$collection->add_to_pointy_objects($pen)};
if ($@) { print $@ }
ok( !$@, 'many_to_many add_to_$rel($object) with where in relationship attrs did not throw');
is($pointy_objects->count, $pointy_count+1, 'many_to_many add_to_$rel($object) with where in relationship attrs count correct');
$pointy_count = $pointy_objects->count();

my $round_objects = $collection->round_objects();
my $round_count = $round_objects->count();
eval {$collection->add_to_objects({ value => "Wheel", type => "round" })};
if ($@) { print $@ }
ok( !$@, 'many_to_many add_to_$rel($hash) did not throw');
is($round_objects->count, $round_count+1, 'many_to_many add_to_$rel($hash) count correct');

# test set_$rel
$round_count = $round_objects->count();
$pointy_count = $pointy_objects->count();
my @all_pointy_objects = $pointy_objects->all;
# doing a set on pointy objects with its current set should not change any counts
eval {$collection->set_pointy_objects(\@all_pointy_objects)};
if ($@) { print $@ }
ok( !$@, 'many_to_many set_$rel(\@objects) did not throw');
is($pointy_objects->count, $pointy_count, 'many_to_many set_$rel($hash) count correct');
is($round_objects->count, $round_count, 'many_to_many set_$rel($hash) other rel count correct');
