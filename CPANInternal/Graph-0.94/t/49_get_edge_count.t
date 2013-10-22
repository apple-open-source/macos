use Test::More tests => 12;

use Graph;
my $g = Graph->new;

is( $g->get_edge_count("a", "b"), 0 );
is( $g->get_edge_count("b", "c"), 0 );

$g->add_edge("a", "b");

is( $g->get_edge_count("a", "b"), 1 );
is( $g->get_edge_count("b", "c"), 0 );

$g->add_edge("a", "b");

is( $g->get_edge_count("a", "b"), 1 );
is( $g->get_edge_count("b", "c"), 0 );

my $h = $g->new(countedged => 1);

$h->add_edge("a", "b");
$h->add_edge("a", "b");

is( $h->get_edge_count("a", "b"), 2 );
is( $h->get_edge_count("b", "c"), 0 );

$h->delete_edge("a", "b");

is( $h->get_edge_count("a", "b"), 1 );
is( $h->get_edge_count("b", "c"), 0 );

$h->delete_edge("a", "b");

is( $h->get_edge_count("a", "b"), 0 );
is( $h->get_edge_count("b", "c"), 0 );

