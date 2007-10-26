use Test::More tests => 4;

use strict;
use warnings;

use Graph::Directed;

my $graph = new Graph::Directed;

$graph->add_edge(qw(a b));
$graph->add_edge(qw(b c));

is($graph, "a-b,b-c");
is(show_successors($graph), "a: b; b: c; c: ");

$graph->set_vertex_attribute('b','label','bla');

is($graph, "a-b,b-c");
is(show_successors($graph), "a: b; b: c; c: ");

sub show_successors {
    my $graph = shift;
    my @v;
    foreach my $v (sort $graph->vertices()) {
	push @v, "$v: " . join(" ", sort $graph->successors($v));
    }
    return join("; ", @v);
}
