use Test::More tests => 15;

use Graph;
my $g = Graph->new(hyperedged => 1);

$g->add_edge();
$g->add_edge("a");
$g->add_edge("b", "c");
$g->add_edge("d", "e", "f");

is( $g->edges, 4 );

my @e = $g->edges;

is ( "@{[ sort map { qq'[@$_]' } @e ]}", "[] [a] [b c] [d e f]" );

ok( $g->has_edge() );
ok( $g->has_edge("a") );
ok( $g->has_edge("b", "c") );
ok( $g->has_edge("d", "e", "f") );

ok( ! $g->has_edge("b") );
ok( ! $g->has_edge("c") );
ok( ! $g->has_edge("c", "b") );
ok( ! $g->has_edge("d", "e") );

$g->add_edge("d", "e", "g");

is( $g->delete_edge("d", "e", "f"), $g );

ok( ! $g->has_edge("d", "e", "f") );
ok(   $g->has_edge("d", "e", "g") );

is( $g->delete_edge("d", "e", "f"), $g );

is ( "@{[ sort map { qq'[@$_]' } $g->edges ]}", "[] [a] [b c] [d e g]" );

