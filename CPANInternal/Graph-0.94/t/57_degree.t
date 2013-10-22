use Test::More tests => 50;

use Graph;
my $g0 = Graph->new;
my $g1 = Graph->new(undirected => 1);

$g0->add_edge(1=>1); $g1->add_edge(1=>1);
$g0->add_edge(1=>2); $g1->add_edge(1=>2);
$g0->add_edge(1=>3); $g1->add_edge(1=>3);
$g0->add_edge(2=>4); $g1->add_edge(2=>4);
$g0->add_edge(5=>4); $g1->add_edge(5=>4);
$g0->add_vertex(6);  $g1->add_vertex(6);  

is( $g0->in_degree(1), 1 );
is( $g0->in_degree(2), 1 );
is( $g0->in_degree(3), 1 );
is( $g0->in_degree(4), 2 );
is( $g0->in_degree(5), 0 );
is( $g0->in_degree(6), 0 );

is( $g0->out_degree(1), 3 );
is( $g0->out_degree(2), 1 );
is( $g0->out_degree(3), 0 );
is( $g0->out_degree(4), 0 );
is( $g0->out_degree(5), 1 );
is( $g0->out_degree(6), 0 );

is( $g0->degree(1), -2 );
is( $g0->degree(2), 0 );
is( $g0->degree(3), 1 );
is( $g0->degree(4), 2 );
is( $g0->degree(5), -1 );
is( $g0->degree(6), 0 );

is( $g0->vertex_degree(1), $g0->degree(1) );
is( $g0->vertex_degree(2), $g0->degree(2) );
is( $g0->vertex_degree(3), $g0->degree(3) );
is( $g0->vertex_degree(4), $g0->degree(4) );
is( $g0->vertex_degree(5), $g0->degree(5) );
is( $g0->vertex_degree(6), $g0->degree(6) );

is( $g1->in_degree(1), 4 );
is( $g1->in_degree(2), 2 );
is( $g1->in_degree(3), 1 );
is( $g1->in_degree(4), 2 );
is( $g1->in_degree(5), 1 );
is( $g1->in_degree(6), 0 );

is( $g1->out_degree(1), 4 );
is( $g1->out_degree(2), 2 );
is( $g1->out_degree(3), 1 );
is( $g1->out_degree(4), 2 );
is( $g1->out_degree(5), 1 );
is( $g1->out_degree(6), 0 );

is( $g1->degree(1), 4 );
is( $g1->degree(2), 2 );
is( $g1->degree(3), 1 );
is( $g1->degree(4), 2 );
is( $g1->degree(5), 1 );
is( $g1->degree(6), 0 );

is( $g1->vertex_degree(1), $g1->degree(1) );
is( $g1->vertex_degree(2), $g1->degree(2) );
is( $g1->vertex_degree(3), $g1->degree(3) );
is( $g1->vertex_degree(4), $g1->degree(4) );
is( $g1->vertex_degree(5), $g1->degree(5) );
is( $g1->vertex_degree(6), $g1->degree(6) );

is( $g0->degree, 0 );
is( $g1->degree, 10 );

