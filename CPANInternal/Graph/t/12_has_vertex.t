use Test::More tests => 7;

use Graph;
my $g = Graph->new;

ok( !$g->has_vertex("a") );
ok( !$g->has_vertex("b") );

ok( !$g->has_vertex("a") );
ok( !$g->has_vertex("b") );

$g->add_vertex("a");

ok(  $g->has_vertex("a") );

ok( !$g->has_vertex("b") );
ok( !$g->has_vertex("b") );

