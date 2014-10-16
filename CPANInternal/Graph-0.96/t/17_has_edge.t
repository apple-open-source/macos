use Test::More tests => 16;

use Graph;
my $g = Graph->new;

ok( !$g->has_edge("a", "b") );
ok( !$g->has_edge("b", "c") );

ok( !$g->has_edge("a", "b") );
ok( !$g->has_edge("b", "c") );

ok(!$g->has_edge(0));
ok(!$g->has_edge(1));
ok(!$g->has_edge(0, 0));
ok(!$g->has_edge(0, 1));
ok(!$g->has_edge(1, 0));
ok(!$g->has_edge(1, 1));
$g->add_edge(0, 1);
ok(!$g->has_edge(0));
ok(!$g->has_edge(1));
ok(!$g->has_edge(0, 0));
ok( $g->has_edge(0, 1));
ok(!$g->has_edge(1, 0));
ok(!$g->has_edge(1, 1));


