use Test::More tests => 5;

use Graph;
my $g = Graph->new;

ok( $g->add_edge("a", "b") );
ok( $g->add_edge("b", "c") );

is( $g->add_edge("c", "d"), $g );

eval '$g->add_edge("c", "d", "e", "f")';
like($@,
     qr/Graph::add_edge: graph is not hyperedged/);

eval '$g->add_edge("c")';
like($@,
     qr/Graph::add_edge: graph is not hyperedged/);

