# rt.cpan.org #31608: Graph::Undirected, unionfind and connected_component

use strict;

use Test::More tests => 2;

use Graph::Undirected;
sub fill_graph
{
my $graph = shift;
$graph->add_edge('A', 'B');
$graph->add_vertex('A');
}
my $graph1 = Graph::Undirected->new('unionfind' => 1);
fill_graph($graph1);
is($graph1->connected_components(), 1);
my $graph2 = Graph::Undirected->new('unionfind' => 0);
fill_graph($graph2);
is($graph2->connected_components(), 1);

