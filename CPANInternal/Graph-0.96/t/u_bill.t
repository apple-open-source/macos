use Test::More tests => 30;

use strict;
use Graph;

my$g = new Graph::Undirected;
$g->add_edges(qw(a1 b1 b1 c1 c1 a1 a2 b2 b2 c2 c2 a2 a1 a2));
$g->add_vertices(1..5);

foreach (1..10) {
    my @b = $g->bridges;
    is(@b, 1);
    my ($u, $v) = sort @{ $b[0] };
    is($u, "a1");
    is($v, "a2");
}
