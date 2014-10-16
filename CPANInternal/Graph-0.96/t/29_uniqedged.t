use Test::More tests => 8;

use Graph;

my $g0 = Graph->new(hyperedged => 1, uniqedged => 1);

ok( $g0->uniqedged);

ok( $g0->add_edge('a', 'a', 'b'));
ok( $g0->has_edge('a', 'a', 'b'));
ok( $g0->has_edge('a', 'b'));

my $g1 = Graph->new(hyperedged => 1, uniqedged => 0);

ok(!$g1->uniqedged);

ok( $g1->add_edge('a', 'a', 'b'));
ok( $g1->has_edge('a', 'a', 'b'));
ok(!$g1->has_edge('a', 'b'));


