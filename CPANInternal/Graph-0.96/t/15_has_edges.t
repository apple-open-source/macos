use Test::More tests => 3;

use Graph;
my $g = Graph->new;

ok( !$g->has_edges() );

$g->add_edge("a", "b");

ok( $g->has_edges() );

$g->add_edge("b", "c");

ok( $g->has_edges() );

