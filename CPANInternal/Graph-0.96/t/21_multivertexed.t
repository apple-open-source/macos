use Test::More tests => 57;

use Graph;
my $g = Graph->new(multivertexed => 1);

is( $g->get_vertex_count('a'), 0 );

ok( $g->add_vertex_by_id('a', 'red') );

is( $g->get_vertex_count('a'), 1 );

ok( $g->has_vertex('a') );
ok(!$g->has_vertex('b') );

ok( $g->has_vertex('a') );
ok(!$g->has_vertex('b') );

ok( $g->has_vertex_by_id('a', 'red') );
ok(!$g->has_vertex_by_id('a', 'blue') );

ok( $g->has_vertex_by_id('a', 'red') );
ok(!$g->has_vertex_by_id('a', 'blue') );

$g->add_vertex_by_id('a', 'blue');

is( $g->get_vertex_count('a'), 2 );

ok( $g->has_vertex_by_id('a', 'blue') );
ok( $g->has_vertex_by_id('a', 'red') );

$g->add_vertex('a');
ok( $g->has_vertex('a') );
ok(!$g->has_vertex('b') );

is( $g->get_vertex_count('a'), 3 );

is( $g->add_vertex_get_id('a'), 1);
is( $g->add_vertex_get_id('a'), 2);
is( $g->add_vertex_get_id('a'), 3);

is( $g->get_vertex_count('a'), 6 );

ok( $g->delete_vertex_by_id('a', 'blue') );

ok(!$g->has_vertex_by_id('a', 'blue') );
ok( $g->has_vertex_by_id('a', 'red') );

ok(!$g->delete_vertex_by_id('a', 'green') );

ok(!$g->has_vertex_by_id('a', 'blue') );
ok( $g->has_vertex_by_id('a', 'red') );
ok(!$g->has_vertex_by_id('a', 'green') );

ok( $g->delete_vertex_by_id('a', 'red') );

my @i = sort $g->get_multivertex_ids('a');

is("@i", "0 1 2 3");

ok( $g->has_vertex_by_id('a', '0') );
ok( $g->has_vertex_by_id('a', '1') );
ok( $g->has_vertex_by_id('a', '2') );
ok( $g->has_vertex_by_id('a', '3') );

is( $g->get_vertex_count('a'), 4 );

is( $g->delete_vertex('a'), '' );

ok(!$g->has_vertex_by_id('a', '0') );
ok(!$g->has_vertex_by_id('a', '1') );
ok(!$g->has_vertex_by_id('a', '2') );
ok(!$g->has_vertex_by_id('a', '3') );

is( $g->get_multivertex_ids('a'), undef );

my $h = Graph->new;

eval '$h->add_vertex_by_id("b", "black")';
like($@, qr/add_vertex_by_id: expected multivertexed/);

eval '$h->has_vertex_by_id("b", "black")';
like($@, qr/has_vertex_by_id: expected multivertexed/);

eval '$h->get_multivertex_ids()';
like($@, qr/get_multivertex_ids: expected multivertexed/);

eval '$h->delete_vertex_by_id("b", "black")';
like($@, qr/delete_vertex_by_id: expected multivertexed/);

$h = Graph->new(multivertexed => 1, hypervertexed => 1);

ok( $h->add_vertex_by_id('u', 'v', 'genghis') );
ok( $h->add_vertex_by_id('khan') );

ok(!$h->has_vertex('u' ,'v', 'w') );
ok( $h->has_vertex('u' ,'v') );
ok(!$h->has_vertex('u') );
ok(!$h->has_vertex('v') );
ok( $h->has_vertex() );

ok( $h->has_vertex_by_id('u', 'v', 'genghis') );
ok( $h->has_vertex_by_id('khan') );

my $g1 = Graph->new;

ok ( !$g1->multivertexed );

my $g2 = Graph->new( multivertexed => 1 );

ok (  $g2->multivertexed );

eval 'my $g3 = Graph->new( multivertexed => 1, countvertexed => 1 )';

like ( $@, qr/both countvertexed and multivertexed/ );
