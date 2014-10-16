use Test::More tests => 28;

use Graph;

my $g1 = Graph->new;

ok ( !$g1->countedged );

my $g2 = Graph->new( countedged => 1 );

ok (  $g2->countedged );

is( $g2->edges, 0 );
is( $g2->unique_edges, 0 );

ok( $g2->add_edge('a', 'b') );
is( $g2->edges, 1 );
is( $g2->unique_edges, 1 );

ok( $g2->add_edge('a', 'b') );
is( $g2->edges, 2 );
is( $g2->unique_edges, 1 );

ok( $g2->add_edge('b', 'c') );
is( $g2->edges, 3 );
is( $g2->unique_edges, 2 );

ok( $g2->add_edge('a', 'b') );
is( $g2->edges, 4 );
is( $g2->unique_edges, 2 );

ok( $g2->delete_edge('b', 'c') );
is( $g2->edges, 3 );
is( $g2->unique_edges, 1 );

ok( $g2->delete_edge('a', 'b') );
is( $g2->edges, 2 );
is( $g2->unique_edges, 1 );

ok( $g2->delete_edge('a', 'b') );
is( $g2->edges, 1 );
is( $g2->unique_edges, 1 );

ok( $g2->delete_edge('a', 'b') );
is( $g2->edges, 0 );
is( $g2->unique_edges, 0 );

