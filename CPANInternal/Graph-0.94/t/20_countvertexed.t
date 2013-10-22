use Test::More tests => 28;

use Graph;

my $g1 = Graph->new;

ok ( !$g1->countvertexed );

my $g2 = Graph->new( countvertexed => 1 );

ok (  $g2->countvertexed );

is( $g2->vertices, 0 );
is( $g2->unique_vertices, 0 );

ok( $g2->add_vertex('a') );
is( $g2->vertices, 1 );
is( $g2->unique_vertices, 1 );

ok( $g2->add_vertex('a') );
is( $g2->vertices, 2 );
is( $g2->unique_vertices, 1 );

ok( $g2->add_vertex('b') );
is( $g2->vertices, 3 );
is( $g2->unique_vertices, 2 );

ok( $g2->add_vertex('a') );
is( $g2->vertices, 4 );
is( $g2->unique_vertices, 2 );

ok( $g2->delete_vertex('b') );
is( $g2->vertices, 3 );
is( $g2->unique_vertices, 1 );

ok( $g2->delete_vertex('a') );
is( $g2->vertices, 2 );
is( $g2->unique_vertices, 1 );

ok( $g2->delete_vertex('a') );
is( $g2->vertices, 1 );
is( $g2->unique_vertices, 1 );

is( $g2->delete_vertex('a'), "" ); # ok($g2->...) fails in 5.00504
is( $g2->vertices, 0 );
is( $g2->unique_vertices, 0 );

