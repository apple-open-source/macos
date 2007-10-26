use Test::More tests => 8;

use Graph;
my $g = Graph->new;

$g->add_path("a", "b", "c", "d", "e");
$g->delete_path("a", "b", "c");

ok( ! $g->has_edge("a", "b") );
ok( ! $g->has_edge("b", "c") );
ok(   $g->has_edge("c", "d") );
ok(   $g->has_edge("d", "e") );

my $h = Graph->new(undirected => 1);

$h->add_path("a", "b", "c", "d", "e");
$h->delete_path("a", "b", "c");

ok( ! $h->has_edge("a", "b") );
ok( ! $h->has_edge("b", "c") );
ok(   $h->has_edge("c", "d") );
ok(   $h->has_edge("d", "e") );
