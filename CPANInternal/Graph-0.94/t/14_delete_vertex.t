use Test::More tests => 36;

use Graph;
my $g = Graph->new;

$g->add_vertex("a");
$g->add_vertex("b");

ok(   $g->delete_vertex("b") );
ok(   $g->has_vertex("a") );
ok( ! $g->has_vertex("b") );
ok( ! $g->has_vertex("c") );

ok(   $g->delete_vertex("c") );
ok(   $g->has_vertex("a") );
ok( ! $g->has_vertex("b") );
ok( ! $g->has_vertex("c") );

is(   $g->delete_vertex("a"), "" );
is(   $g->delete_vertex("a"), "" );

$g->add_vertices(qw(a b c d));
ok(   $g->has_vertex("a") );
ok(   $g->has_vertex("b") );
ok(   $g->has_vertex("c") );
ok(   $g->has_vertex("d") );

$g->delete_vertices(qw(a c));
ok( ! $g->has_vertex("a") );
ok(   $g->has_vertex("b") );
ok( ! $g->has_vertex("c") );
ok(   $g->has_vertex("d") );

$g->delete_vertices(qw(a c));
ok( ! $g->has_vertex("a") );
ok(   $g->has_vertex("b") );
ok( ! $g->has_vertex("c") );
ok(   $g->has_vertex("d") );

$g->delete_vertices(qw(b d));
ok( ! $g->has_vertex("a") );
ok( ! $g->has_vertex("b") );
ok( ! $g->has_vertex("c") );
ok( ! $g->has_vertex("d") );

is( $g->delete_vertex(), $g );
is( $g->delete_vertices(), $g );

my $h = Graph->new(countvertexed => 1);

$h->add_vertices(qw(a a b b));
ok(   $h->has_vertex("a") );
ok(   $h->has_vertex("b") );

$h->delete_vertex('a');
ok(   $h->has_vertex("a") );
$h->delete_vertex('a');
ok( ! $h->has_vertex("a") );

$h->delete_vertices('b');
ok(   $h->has_vertices("b") );
$h->delete_vertices('b');
ok( ! $h->has_vertices("b") );

{
    # From Andras Salamon
    use Graph::Directed;
    my $f = new Graph::Directed;
    $f->add_edges( qw( a a a b ) ); # notice self-loop
    is($f, "a-a,a-b");
    $f->delete_vertex('a');
    is($f, "b");
}

