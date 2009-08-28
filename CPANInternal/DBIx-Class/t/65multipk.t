use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 5;

my $artist = $schema->resultset("Artist")->find(1);
ok($artist->find_related('twokeys', {cd => 1}), "find multiple pks using relationships + args");

ok($schema->resultset("FourKeys")->search({ foo => 1, bar => 2 })->find({ hello => 3, goodbye => 4 }), "search on partial key followed by a find");
ok($schema->resultset("FourKeys")->find(1,2,3,4), "find multiple pks without hash");
ok($schema->resultset("FourKeys")->find(5,4,3,6), "find multiple pks without hash");
is($schema->resultset("FourKeys")->find(1,2,3,4)->ID, 'DBICTest::FourKeys|fourkeys|bar=2|foo=1|goodbye=4|hello=3', 'unique object id ok for multiple pks');

