use Test::More tests => 34;

use Graph;
my $g = Graph->new;

ok( $g->add_edge("a", "b") );
ok( $g->add_edge("b", "c") );

ok(   $g->delete_edge("b", "c") );
ok(   $g->has_edge("a", "b") );
ok( ! $g->has_edge("b", "c") );

ok(   $g->delete_edge("b", "d") );
ok(   $g->has_edge("a", "b") );
ok( ! $g->has_edge("b", "c") );

is( $g->delete_edge("a", "b"), 'a,b,c' );
is( $g->delete_edge("a", "b"), 'a,b,c' );

$g->add_edges(qw(a b b x c d c y));
ok(   $g->has_edge("a", "b") );
ok(   $g->has_edge("b", "x") );
ok(   $g->has_edge("c", "d") );
ok(   $g->has_edge("c", "y") );

$g->delete_edges(qw(a b c d));
ok( ! $g->has_edge("a", "b") );
ok(   $g->has_edge("b", "x") );
ok( ! $g->has_edge("c", "d") );
ok(   $g->has_edge("c", "y") );

$g->delete_edges(qw(a b c d));
ok( ! $g->has_edge("a", "b") );
ok(   $g->has_edge("b", "x") );
ok( ! $g->has_edge("c", "d") );
ok(   $g->has_edge("c", "y") );

$g->delete_edges(qw(b x c y));
ok( ! $g->has_edge("a", "b") );
ok( ! $g->has_edge("b", "x") );
ok( ! $g->has_edge("c", "d") );
ok( ! $g->has_edge("c", "y") );

is( $g->delete_edge(), $g );
is( $g->delete_edges(), $g );

my $h = Graph->new(countedged => 1);

$h->add_edges(qw(a x a x b y b y));
ok(   $h->has_edge("a", "x") );
ok(   $h->has_edge("b", "y") );

$h->delete_edge('a', 'x');
ok(   $h->has_edge("a", "x") );
$h->delete_edge('a', 'x');
ok( ! $h->has_edge("a", "x") );

$h->delete_edges('b', 'y');
ok(   $h->has_edges("b", "y") );
$h->delete_edges('b', 'y');
ok( ! $h->has_edges("b", "y") );

