use Test::More tests => 15;

use Graph;

my $g = Graph->new;

is( $g->[1], 0 ); # [1] is the generational index
ok( $g->add_vertex('a') );
is( $g->[1], 1 );
ok( $g->add_vertex('b') );
is( $g->[1], 2 );
ok( $g->add_edge('a', 'b') );
is( $g->[1], 3 );
ok( $g->delete_edge('a', 'b') );
is( $g->[1], 4 );
ok( $g->add_edge('a', 'c') );
is( $g->[1], 6 );
ok( $g->delete_vertex('a') );
is( $g->[1], 7 );
ok( $g->delete_vertex('b') );
is( $g->[1], 8 );

