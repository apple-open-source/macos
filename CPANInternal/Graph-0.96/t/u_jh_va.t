use Test::More tests => 18;

use Graph;
my $g = Graph->new;

$g->add_path('a'..'d');

ok( $g->has_vertex('a'));
ok( $g->has_vertex('b'));
ok( $g->has_vertex('c'));
ok( $g->has_vertex('d'));

$g->delete_vertex('d');

ok( $g->has_vertex('a'));
ok( $g->has_vertex('b'));
ok( $g->has_vertex('c'));
ok(!$g->has_vertex('d'));

$g->set_vertex_attribute('b','a',1);

ok( $g->delete_vertex('b'));

ok( $g->has_vertex('a'));
ok(!$g->has_vertex('b'));
ok( $g->has_vertex('c'));
ok(!$g->has_vertex('d'));

ok( $g->delete_vertex('a'));

ok(!$g->has_vertex('a'));
ok(!$g->has_vertex('b'));
ok( $g->has_vertex('c'));
ok(!$g->has_vertex('d'));
