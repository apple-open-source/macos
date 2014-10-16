use Test::More tests => 18;

use Graph;

my $g0 = Graph->new(directed => 1);
my $g1 = Graph->new(directed => 0);
my $g2 = Graph->new(directed => 1);

$g0->add_edge('a', 'b');
$g1->add_edge('a', 'b');
$g2->add_edge('a', 'a');

eval '$g0->expect_undirected';
like($@, qr/expected undirected graph, got directed/);

eval '$g1->expect_undirected';
is($@, '');

eval '$g0->expect_directed';
is($@, '');

eval '$g1->expect_directed';
like($@, qr/expected directed graph, got undirected/);

eval '$g0->expect_acyclic';
is($@, '');

eval '$g1->expect_acyclic';
is($@, '');

eval '$g2->expect_acyclic';
like($@, qr/expected acyclic graph, got cyclic/);

eval '$g0->expect_dag';
is($@, '');

eval '$g1->expect_dag';
like($@, qr/expected directed acyclic graph, got undirected/);

eval '$g2->expect_dag';
like($@, qr/expected directed acyclic graph, got cyclic/);

eval 'Graph->random_graph(42)';
like($@, qr/Graph::random_graph: argument 'vertices' missing or undef/);

eval 'Graph->random_graph(vertices=>100)';
is($@, '');

eval 'Graph->random_graph(42,43,44)';
like($@, qr/Graph::random_graph: argument 'vertices' missing or undef/);

eval 'Graph::_get_options()';
like($@, qr/internal error: should be called with only one array ref argument/);

eval 'Graph::_get_options(1)';
like($@, qr/internal error: should be called with only one array ref argument/);

eval 'Graph::_get_options([])';
is($@, '');

eval 'Graph::_get_options(12,34)';
like($@, qr/internal error: should be called with only one array ref argument/);

my $uf = Graph->new(undirected => 1, unionfind => 1);
$uf->add_edge(qw(a b));
eval '$uf->delete_edge("a")';
like($@, qr/Graph::delete_edge: expected non-unionfind graph/);
