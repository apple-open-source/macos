use Test::More tests => 16;

use Graph;

my $g0 = Graph->new(hypervertexed => 1, omnivertexed => 1);

ok($g0->omnivertexed);

ok($g0->add_vertex('a', 'b'));

ok( $g0->has_vertex('a')); # Added implicitly
ok( $g0->has_vertex('b')); # Added implicitly
ok( $g0->has_vertex('a', 'a')); # Added (uniquely) implicitly
ok( $g0->has_vertex('a', 'b'));
ok( $g0->has_vertex('b', 'a')); # Omni
ok( $g0->has_vertex('b', 'b')); # Added (uniquely) implicitly

my $g1 = Graph->new(hypervertexed => 1, omnivertexed => 0);

ok(!$g1->omnivertexed);

ok($g1->add_vertex('a', 'b'));

ok( $g1->has_vertex('a')); # Added implicitly
ok( $g1->has_vertex('b')); # Added implicitly
ok( $g1->has_vertex('a', 'a')); # Added (uniquely) implicitly
ok( $g1->has_vertex('a', 'b'));
ok(!$g1->has_vertex('b', 'a')); # !Omni
ok( $g1->has_vertex('b', 'b')); # Added (uniquely) implicitly

