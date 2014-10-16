use Test::More tests => 28;

use Graph;

my $g0 = Graph->new(hypervertexed => 1, omnivertexed => 1, uniqvertexed => 1);

ok($g0->add_vertex('a', 'b'));

ok( $g0->has_vertex('a')); # Added implicitly
ok( $g0->has_vertex('b')); # Added implicitly
ok( $g0->has_vertex('a', 'a')); # Added (uniquely) implicitly
ok( $g0->has_vertex('a', 'b'));
ok( $g0->has_vertex('b', 'a')); # Omni
ok( $g0->has_vertex('b', 'b')); # Added (uniquely) implicitly

my $g1 = Graph->new(hypervertexed => 1, omnivertexed => 0, uniqvertexed => 1);

ok($g1->add_vertex('a', 'b'));

ok( $g1->has_vertex('a')); # Added implicitly
ok( $g1->has_vertex('b')); # Added implicitly
ok( $g1->has_vertex('a', 'a')); # Added (uniquely) implicitly
ok( $g1->has_vertex('a', 'b'));
ok(!$g1->has_vertex('b', 'a')); # !Omni
ok( $g1->has_vertex('b', 'b')); # Added (uniquely) implicitly

my $g2 = Graph->new(hypervertexed => 1, omnivertexed => 1, uniqvertexed => 0);

ok($g2->add_vertex('a', 'b'));

ok( $g2->has_vertex('a')); # Added implicitly
ok( $g2->has_vertex('b')); # Added implicitly
ok(!$g2->has_vertex('a', 'a')); # !Uniq
ok( $g2->has_vertex('a', 'b'));
ok( $g2->has_vertex('b', 'a')); # Omni
ok(!$g2->has_vertex('b', 'b')); # Added (!Uniq) implicitly

my $g3 = Graph->new(hypervertexed => 1, omnivertexed => 0, uniqvertexed => 0);

ok($g3->add_vertex('a', 'b'));

ok( $g3->has_vertex('a')); # Added implicitly
ok( $g3->has_vertex('b')); # Added implicitly
ok(!$g3->has_vertex('a', 'a')); # Added (!Uniq) implicitly
ok( $g3->has_vertex('a', 'b'));
ok(!$g3->has_vertex('b', 'a')); # !Omni
ok(!$g3->has_vertex('b', 'b')); # Added (!uniq) implicitly

