use Test::More tests => 10;

use Graph;
my $g = Graph->new;

ok( !$g->has_edges() );

my $v = $g->edges;

is( $v, 0 );

my @v = $g->edges;

is( "@v", "" );

my ($e, @e);

$g->add_edge("a", "b");

$e = $g->edges;

is( $e, 1 );

@e = map { "[@$_]" } $g->edges;

is( "@e", "[a b]" );

$g->add_edge("b", "c");

$e = $g->edges;

is( $e, 2 );

@e = sort map { "[@$_]" } $g->edges;

is( "@e", "[a b] [b c]" );

eval '$g->add_edges("x")';
like($@, qr/Graph::add_edges: missing end vertex/);

is($g->add_edges("x", "y"), $g);

is($g, "a-b,b-c,x-y");


