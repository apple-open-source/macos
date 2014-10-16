# rt.cpan.org #41190: add_edge_by_id on multigraph malfunctioning

use strict;

use Test::More tests => 4;

use Graph;

my $G0 = Graph->new(undirected => 1,
		    vertices => ["v0", "v1", "v2"],
		    multiedged => 1);
$G0->add_edge_by_id("v0", "v1", "0");

is($G0, "v0=v1,v2");

my $G1 = Graph->new(undirected => 1,
		    vertices => ["v0", "v1", "v2"],
		    multiedged => 1);
$G1->add_edge_by_id("v0", "v1", "1");

is($G1, "v0=v1,v2");

my $G2 = Graph->new(undirected => 1,
		    vertices => ["v0", "v1", "v2"],
		    multiedged => 1);
$G2->add_edge_by_id("v0", "v0", "0");

is($G2, "v0=v0,v1,v2");

my $G3 = Graph->new(undirected => 1,
		    vertices => ["v0", "v1", "v2"],
		    multiedged => 1);
$G3->add_edge_by_id("v0", "v0", "1");

is($G3, "v0=v0,v1,v2");

