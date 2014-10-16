use Test::More tests => 10;

use Graph;

my $g0 = Graph->new(hypervertexed => 1, uniqvertexed => 1);

ok($g0->uniqvertexed);

ok($g0->add_vertex('a', 'a'));

ok($g0->has_vertex('a'));
ok($g0->has_vertex('a', 'a'));
ok($g0->has_vertex('a', 'a', 'a'));

my $g1 = Graph->new(hypervertexed => 1, uniqvertexed => 0);

ok(!$g1->uniqvertexed);

ok($g1->add_vertex('a', 'a'));

ok( $g1->has_vertex('a')); # Added implicitly
ok( $g1->has_vertex('a', 'a'));
ok(!$g1->has_vertex('a', 'a', 'a'));


