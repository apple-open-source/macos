use Test::More tests => 12;

use Graph;
my $g = Graph->new;

is( $g->get_vertex_count("a"), 0 );
is( $g->get_vertex_count("b"), 0 );

$g->add_vertex("a");

is( $g->get_vertex_count("a"), 1 );
is( $g->get_vertex_count("b"), 0 );

$g->add_vertex("a");

is( $g->get_vertex_count("a"), 1 );
is( $g->get_vertex_count("b"), 0 );

my $h = $g->new(countvertexed => 1);

$h->add_vertex("a");
$h->add_vertex("a");

is( $h->get_vertex_count("a"), 2 );
is( $h->get_vertex_count("b"), 0 );

$h->delete_vertex("a");

is( $h->get_vertex_count("a"), 1 );
is( $h->get_vertex_count("b"), 0 );

$h->delete_vertex("a");

is( $h->get_vertex_count("a"), 0 );
is( $h->get_vertex_count("b"), 0 );

