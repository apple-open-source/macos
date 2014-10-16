use Graph::Undirected;

use Test::More tests => 2;

my $mg = new Graph::Undirected;
$mg->add_edges(qw(C6 H6A S1 C6));

is($mg->connected_component_by_vertex("S1"), 0);
my @cc0 = $mg->connected_component_by_index(0);
my %cc0; @cc0{ @cc0 } = ();
ok(exists $cc0{ S1 });
