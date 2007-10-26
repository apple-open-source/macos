use Test::More tests => 209;

use Graph::Undirected;
use Graph::Directed;

my $g0 = Graph::Undirected->new;

ok(!$g0->is_connected);
is( $g0->connected_components(), 0);
is( $g0->connected_component_by_vertex('a'), undef);
is( $g0->connected_component_by_index(0), undef );
ok(!$g0->same_connected_components('a', 'b'));
is($g0->connected_graph, '');

$g0->add_vertex('a');

ok( $g0->is_connected);
is( $g0->connected_components(), 1);
isnt($g0->connected_component_by_vertex('a'), undef);
is( "@{[ $g0->connected_component_by_index(0) ]}", 'a' );
ok(!$g0->same_connected_components('a', 'b'));
is($g0->connected_graph, 'a');

$g0->add_vertex('b');

ok(!$g0->is_connected);
is( $g0->connected_components(), 2);
isnt($g0->connected_component_by_vertex('a'), undef);
isnt($g0->connected_component_by_vertex('b'), undef);
isnt($g0->connected_component_by_vertex('a'),
     $g0->connected_component_by_vertex('b'));
my @c0a = $g0->connected_component_by_index(0);
my @c0b = $g0->connected_component_by_index(0);
my @c0c = $g0->connected_component_by_index(0);
is( @c0a, 1 );
is( @c0b, 1 );
is( @c0c, 1 );
is( "@c0a", "@c0b" );
is( "@c0a", "@c0c" );
my @c1a = $g0->connected_component_by_index(1);
my @c1b = $g0->connected_component_by_index(1);
my @c1c = $g0->connected_component_by_index(1);
is( @c1a, 1 );
is( @c1b, 1 );
is( @c1c, 1 );
is( "@c1a", "@c1b" );
is( "@c1a", "@c1c" );
isnt( "@c0a", "@c1a" );
ok( ("@c0a" eq "a" && "@c1a" eq "b") ||
    ("@c0a" eq "b" && "@c1a" eq "a") );
ok(!$g0->same_connected_components('a', 'b'));
is($g0->connected_graph, 'a,b');

$g0->add_edge(qw(a b));

ok( $g0->is_connected);
is( $g0->connected_components(), 1);
isnt($g0->connected_component_by_vertex('a'), undef);
isnt($g0->connected_component_by_vertex('b'), undef);
is($g0->connected_component_by_vertex('a'), $g0->connected_component_by_vertex('b'));
@c0a = $g0->connected_component_by_index(0);
@c0b = $g0->connected_component_by_index(0);
@c0c = $g0->connected_component_by_index(0);
is( @c0a, 2 );
is( @c0b, 2 );
is( @c0c, 2 );
is( "@c0a", "@c0b" );
is( "@c0a", "@c0c" );
@c1a = $g0->connected_component_by_index(1);
@c1b = $g0->connected_component_by_index(1);
@c1c = $g0->connected_component_by_index(1);
is( @c1a, 0 );
is( @c1b, 0 );
is( @c1c, 0 );
ok( "@{[ sort @c0a ]}" eq "a b" );
ok( $g0->same_connected_components('a', 'b'));
is($g0->connected_graph, 'a+b');

$g0->add_edge(qw(c d));

ok(!$g0->is_connected);
is( $g0->connected_components(), 2);
isnt($g0->connected_component_by_vertex('a'), undef);
isnt($g0->connected_component_by_vertex('b'), undef);
isnt($g0->connected_component_by_vertex('c'), undef);
isnt($g0->connected_component_by_vertex('d'), undef);
is($g0->connected_component_by_vertex('a'), $g0->connected_component_by_vertex('b'));
is($g0->connected_component_by_vertex('c'), $g0->connected_component_by_vertex('d'));
isnt($g0->connected_component_by_vertex('a'), $g0->connected_component_by_vertex('d'));
ok( $g0->same_connected_components('a', 'b'));
ok( $g0->same_connected_components('c', 'd'));
ok(!$g0->same_connected_components('a', 'c'));
is($g0->connected_graph, 'a+b,c+d');

my $g1 = Graph::Undirected->new(unionfind => 1);

ok(!$g1->is_connected);
is( $g1->connected_components(), 0);
is( $g1->connected_component_by_vertex('a'), undef);
ok(!$g1->same_connected_components('a', 'b'));
is($g1->connected_graph, '');

$g1->add_vertex('a');

ok( $g1->is_connected);
is( $g1->connected_components(), 1);
isnt($g1->connected_component_by_vertex('a'), undef);
ok(!$g1->same_connected_components('a', 'b'));
is($g1->connected_graph, 'a');

$g1->add_vertex('b');

ok(!$g1->is_connected);
is( $g1->connected_components(), 2);
isnt($g1->connected_component_by_vertex('a'), undef);
isnt($g1->connected_component_by_vertex('b'), undef);
isnt($g1->connected_component_by_vertex('a'), $g1->connected_component_by_vertex('b'));
ok(!$g1->same_connected_components('a', 'b'));
is($g1->connected_graph, 'a,b');

$g1->add_edge(qw(a b));

ok( $g1->is_connected);
is( $g1->connected_components(), 1);
isnt($g1->connected_component_by_vertex('a'), undef);
isnt($g1->connected_component_by_vertex('b'), undef);
is($g1->connected_component_by_vertex('a'), $g1->connected_component_by_vertex('b'));
ok( $g1->same_connected_components('a', 'b'));
is($g1->connected_graph, 'a+b');

$g1->add_edge(qw(c d));

ok(!$g1->is_connected);
is( $g1->connected_components(), 2);
isnt($g1->connected_component_by_vertex('a'), undef);
isnt($g1->connected_component_by_vertex('b'), undef);
isnt($g1->connected_component_by_vertex('c'), undef);
isnt($g1->connected_component_by_vertex('d'), undef);
is($g1->connected_component_by_vertex('a'), $g1->connected_component_by_vertex('b'));
is($g1->connected_component_by_vertex('c'), $g1->connected_component_by_vertex('d'));
isnt($g1->connected_component_by_vertex('a'), $g1->connected_component_by_vertex('d'));
ok( $g1->same_connected_components('a', 'b'));
ok( $g1->same_connected_components('c', 'd'));
ok(!$g1->same_connected_components('a', 'c'));
is($g1->connected_graph, 'a+b,c+d');

my $g2 = Graph::Directed->new;

ok(!$g2->is_weakly_connected);
is( $g2->weakly_connected_components(), 0);
is( $g2->weakly_connected_component_by_vertex('a'), undef);
is( $g2->weakly_connected_component_by_index(0), undef );
ok(!$g2->same_weakly_connected_components('a', 'b'));
is($g2->weakly_connected_graph, '');

$g2->add_vertex('a');

ok( $g2->is_weakly_connected);
is( $g2->weakly_connected_components(), 1);
isnt($g2->weakly_connected_component_by_vertex('a'), undef);
is( "@{[ $g2->weakly_connected_component_by_index(0) ]}", 'a' );
ok(!$g2->same_weakly_connected_components('a', 'b'));
is($g2->weakly_connected_graph, 'a');

$g2->add_vertex('b');

ok(!$g2->is_weakly_connected);
is( $g2->weakly_connected_components(), 2);
isnt($g2->weakly_connected_component_by_vertex('a'), undef);
isnt($g2->weakly_connected_component_by_vertex('b'), undef);
isnt($g2->weakly_connected_component_by_vertex('a'), $g2->weakly_connected_component_by_vertex('b'));
@c0a = $g2->weakly_connected_component_by_index(0);
@c0b = $g2->weakly_connected_component_by_index(0);
@c0c = $g2->weakly_connected_component_by_index(0);
is( @c0a, 1 );
is( @c0b, 1 );
is( @c0c, 1 );
is( "@c0a", "@c0b" );
is( "@c0a", "@c0c" );
@c1a = $g2->weakly_connected_component_by_index(1);
@c1b = $g2->weakly_connected_component_by_index(1);
@c1c = $g2->weakly_connected_component_by_index(1);
is( @c1a, 1 );
is( @c1b, 1 );
is( @c1c, 1 );
is( "@c1a", "@c1b" );
is( "@c1a", "@c1c" );
isnt( "@c0a", "@c1a" );
ok( ("@c0a" eq "a" && "@c1a" eq "b") ||
    ("@c0a" eq "b" && "@c1a" eq "a") );
ok(!$g2->same_weakly_connected_components('a', 'b'));
is($g2->weakly_connected_graph, 'a,b');

$g2->add_edge(qw(a b));

ok( $g2->is_weakly_connected);
is( $g2->weakly_connected_components(), 1);
isnt($g2->weakly_connected_component_by_vertex('a'), undef);
isnt($g2->weakly_connected_component_by_vertex('b'), undef);
is($g2->weakly_connected_component_by_vertex('a'), $g2->weakly_connected_component_by_vertex('b'));
@c0a = $g2->weakly_connected_component_by_index(0);
@c0b = $g2->weakly_connected_component_by_index(0);
@c0c = $g2->weakly_connected_component_by_index(0);
is( @c0a, 2 );
is( @c0b, 2 );
is( @c0c, 2 );
is( "@c0a", "@c0b" );
is( "@c0a", "@c0c" );
@c1a = $g2->weakly_connected_component_by_index(1);
@c1b = $g2->weakly_connected_component_by_index(1);
@c1c = $g2->weakly_connected_component_by_index(1);
is( @c1a, 0 );
is( @c1b, 0 );
is( @c1c, 0 );
ok( "@{[ sort @c0a ]}" eq "a b" );
ok( $g2->same_weakly_connected_components('a', 'b'));
is($g2->weakly_connected_graph, 'a+b');

$g2->add_edge(qw(c d));

ok(!$g2->is_weakly_connected);
is( $g2->weakly_connected_components(), 2);
isnt($g2->weakly_connected_component_by_vertex('a'), undef);
isnt($g2->weakly_connected_component_by_vertex('b'), undef);
isnt($g2->weakly_connected_component_by_vertex('c'), undef);
isnt($g2->weakly_connected_component_by_vertex('d'), undef);
is($g2->weakly_connected_component_by_vertex('a'), $g2->weakly_connected_component_by_vertex('b'));
is($g2->weakly_connected_component_by_vertex('c'), $g2->weakly_connected_component_by_vertex('d'));
isnt($g2->weakly_connected_component_by_vertex('a'), $g2->weakly_connected_component_by_vertex('d'));
ok( $g2->same_weakly_connected_components('a', 'b'));
ok( $g2->same_weakly_connected_components('c', 'd'));
ok(!$g2->same_weakly_connected_components('a', 'c'));
is($g2->weakly_connected_graph, 'a+b,c+d');

my $g3 = Graph::Undirected->new(unionfind => 1, multiedged => 1);

ok(!$g3->is_connected);
is( $g3->connected_components(), 0);
is( $g3->connected_component_by_vertex('a'), undef);
ok(!$g3->same_connected_components('a', 'b'));
is($g3->connected_graph, '');

$g3->add_vertex('a');

ok( $g3->is_connected);
is( $g3->connected_components(), 1);
isnt($g3->connected_component_by_vertex('a'), undef);
ok(!$g3->same_connected_components('a', 'b'));
is($g3->connected_graph, 'a');

$g3->add_vertex('b');

ok(!$g3->is_connected);
is( $g3->connected_components(), 2);
isnt($g3->connected_component_by_vertex('a'), undef);
isnt($g3->connected_component_by_vertex('b'), undef);
isnt($g3->connected_component_by_vertex('a'), $g3->connected_component_by_vertex('b'));
ok(!$g3->same_connected_components('a', 'b'));
is($g3->connected_graph, 'a,b');

$g3->add_edge(qw(a b));

ok( $g3->is_connected);
is( $g3->connected_components(), 1);
isnt($g3->connected_component_by_vertex('a'), undef);
isnt($g3->connected_component_by_vertex('b'), undef);
is($g3->connected_component_by_vertex('a'), $g3->connected_component_by_vertex('b'));
ok( $g3->same_connected_components('a', 'b'));
is($g3->connected_graph, 'a+b');

$g3->add_edge(qw(c d));

ok(!$g3->is_connected);
is( $g3->connected_components(), 2);
isnt($g3->connected_component_by_vertex('a'), undef);
isnt($g3->connected_component_by_vertex('b'), undef);
isnt($g3->connected_component_by_vertex('c'), undef);
isnt($g3->connected_component_by_vertex('d'), undef);
is($g3->connected_component_by_vertex('a'), $g3->connected_component_by_vertex('b'));
is($g3->connected_component_by_vertex('c'), $g3->connected_component_by_vertex('d'));
isnt($g3->connected_component_by_vertex('a'), $g3->connected_component_by_vertex('d'));
ok( $g3->same_connected_components('a', 'b'));
ok( $g3->same_connected_components('c', 'd'));
ok(!$g3->same_connected_components('a', 'c'));

my $g3c = $g3->connected_graph;
is($g3c, 'a+b,c+d');

is("@{[sort @{ $g3c->get_vertex_attribute('a+b', 'subvertices') }]}", "a b");
is("@{[sort @{ $g3c->get_vertex_attribute('c+d', 'subvertices') }]}", "c d");
is($g3c->get_vertex_attribute('b+a', 'subvertices'), undef);

my $g4 = Graph::Directed->new;

eval '$g4->is_connected';
like($@, qr/Graph::is_connected: expected undirected graph, got directed/);

eval '$g4->connected_components';
like($@, qr/Graph::connected_components: expected undirected graph, got directed/);

eval '$g4->connected_component_by_vertex';
like($@, qr/Graph::connected_component_by_vertex: expected undirected graph, got directed/);

eval '$g4->connected_component_by_index';
like($@, qr/Graph::connected_component_by_index: expected undirected graph, got directed/);

eval '$g4->same_connected_components';
like($@, qr/Graph::same_connected_components: expected undirected graph, got directed/);

eval '$g4->connected_graph';
like($@, qr/Graph::connected_graph: expected undirected graph, got directed/);

my $g5 = Graph::Undirected->new;

eval '$g5->is_weakly_connected';
like($@, qr/Graph::is_weakly_connected: expected directed graph, got undirected/);

eval '$g5->weakly_connected_components';
like($@, qr/Graph::weakly_connected_components: expected directed graph, got undirected/);

eval '$g5->weakly_connected_component_by_vertex';
like($@, qr/Graph::weakly_connected_component_by_vertex: expected directed graph, got undirected/);

eval '$g5->weakly_connected_component_by_index';
like($@, qr/Graph::weakly_connected_component_by_index: expected directed graph, got undirected/);

eval '$g5->same_weakly_connected_components';
like($@, qr/Graph::same_weakly_connected_components: expected directed graph, got undirected/);

eval '$g5->weakly_connected_graph';
like($@, qr/Graph::weakly_connected_graph: expected directed graph, got undirected/);
