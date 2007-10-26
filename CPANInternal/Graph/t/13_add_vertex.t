use Test::More tests => 4;

use Graph;
my $g = Graph->new;

ok( $g->add_vertex("a") );
ok( $g->add_vertex("b") );

is( $g->add_vertex("c"), $g );

eval '$g->add_vertex("c", "d")';
like($@,
     qr/Graph::add_vertex: use add_vertices for more than one vertex/);


