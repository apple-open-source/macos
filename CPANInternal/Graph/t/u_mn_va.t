use Test::More tests => 3;
use Graph::Undirected;

my $G = Graph::Undirected->new;
$G->add_vertex('a');
$G->set_vertex_attribute('a', 'sim', 0);
$G->add_vertex('b');
$G->set_vertex_attribute('b', 'sim', 1);
$G->add_edge('a', 'b');
$G->delete_vertex('a');
ok(! $G->has_vertex('a') );
my @V = $G->vertices;
is(scalar @V, 1);
my %V; @V{ @V } = ();
ok(exists $V{b});

