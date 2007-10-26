use Test::More tests => 13;

use Graph;
my $g = Graph->new(hypervertexed => 1);

$g->add_vertex();
$g->add_vertex("a");
$g->add_vertex("b", "c");

is( $g->vertices, 5 ); # [], [a], [b], [c], [b, c]

my @v = $g->vertices;

is ( "@{[ sort map { qq'[@$_]' } @v ]}", "[] [a] [b c] [b] [c]" );

ok(   $g->has_vertex() );
ok(   $g->has_vertex("a") );
ok(   $g->has_vertex("b", "c") );

ok(   $g->has_vertex("b") );
ok(   $g->has_vertex("c") );
ok(   $g->has_vertex("c", "b") );

$g->add_vertex("b", "d");

ok( $g->delete_vertex("b", "c") );

ok( ! $g->has_vertex("b", "c") );
ok(   $g->has_vertex("b", "d") );

is(   $g->delete_vertex("b", "c"), $g );

is ( "@{[ sort map { qq'[@$_]' } $g->vertices ]}", "[] [a] [b d] [b] [c] [d]" );

