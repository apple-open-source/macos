use Test::More tests => 12;

use Graph;
my $g = Graph->new;

$g->add_path("a", "b", "c");

ok(   $g->has_edge("a", "b") );
ok( ! $g->has_edge("a", "c") ); # @todo: hyperedges
ok( ! $g->has_edge("b", "a") );
ok(   $g->has_edge("b", "c") );
ok( ! $g->has_edge("c", "a") );
ok( ! $g->has_edge("c", "b") );

my $h = Graph->new(undirected => 1);

$h->add_path("a", "b", "c");

ok(   $h->has_edge("a", "b") );
ok( ! $h->has_edge("a", "c") ); # @todo: hyperedges
ok(   $h->has_edge("b", "a") );
ok(   $h->has_edge("b", "c") );
ok( ! $h->has_edge("c", "a") ); # @todo: hyperedges
ok(   $h->has_edge("c", "b") );

