use Test::More tests => 18;

use Graph;
my $g = Graph->new(hypervertexed => 1);

ok( $g->add_edge(["a","b"],"c") );
ok( $g->add_edge("a",["b","c"]) );

is( $g->vertices, 5 ); # b implicitly added
is( $g->edges,    2 );

ok( $g->has_vertex("a","b") );
ok( $g->has_vertex("c")     );
ok( $g->has_vertex("a")     );
ok( $g->has_vertex("b","c") );

ok( $g->has_edge(["a","b"],"c") );
ok( $g->has_edge("a",["b","c"]) );

ok(   $g->delete_edge("a",["b","c"]) );
ok( ! $g->has_edge   ("a",["b","c"]) );

ok(   $g->delete_edge("a",["b","d"]) );
ok( ! $g->has_edge   ("a",["b","d"]) );

ok(   $g->delete_vertex("c")      );
ok( ! $g->has_edge(["a","b"],"c") );

is( $g->vertices, 4 );
is( $g->edges,    0 );

