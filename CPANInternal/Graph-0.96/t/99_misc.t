use strict;

use Test::More tests => 25;

use Graph::Directed;
use Graph::Undirected;

my $g0 = Graph::Directed->new;

$g0->add_edge(qw(a b));
$g0->add_edge(qw(a c));
$g0->add_edge(qw(b d));
$g0->add_edge(qw(b e));
$g0->add_edge(qw(c f));
$g0->add_edge(qw(c g));

my $da0 = $g0->subgraph_by_radius('a', 0);
my $da1 = $g0->subgraph_by_radius('a', 1);
my $da2 = $g0->subgraph_by_radius('a', 2);
my $da3 = $g0->subgraph_by_radius('a', 3);

is($da0, "a");
is($da1, "a-b,a-c");
is($da2, "a-b,a-c,b-d,b-e,c-f,c-g");
is($da3, "a-b,a-c,b-d,b-e,c-f,c-g");

my $db0 = $g0->subgraph_by_radius('b', 0);
my $db1 = $g0->subgraph_by_radius('b', 1);
my $db2 = $g0->subgraph_by_radius('b', 2);
my $db3 = $g0->subgraph_by_radius('b', 3);

is($db0, "b");
is($db1, "b-d,b-e");
is($db2, "b-d,b-e");
is($db3, "b-d,b-e");

my $g1 = Graph::Undirected->new;

$g1->add_edge(qw(a b));
$g1->add_edge(qw(a c));
$g1->add_edge(qw(b d));
$g1->add_edge(qw(b e));
$g1->add_edge(qw(c f));
$g1->add_edge(qw(c g));

my $ua0 = $g1->subgraph_by_radius('a', 0);
my $ua1 = $g1->subgraph_by_radius('a', 1);
my $ua2 = $g1->subgraph_by_radius('a', 2);
my $ua3 = $g1->subgraph_by_radius('a', 3);

is($ua0, "a");
is($ua1, "a=b,a=c");
is($ua2, "a=b,a=c,b=d,b=e,c=f,c=g");
is($ua3, "a=b,a=c,b=d,b=e,c=f,c=g");

my $ub0 = $g1->subgraph_by_radius('b', 0);
my $ub1 = $g1->subgraph_by_radius('b', 1);
my $ub2 = $g1->subgraph_by_radius('b', 2);
my $ub3 = $g1->subgraph_by_radius('b', 3);

is($ub0, "b");
is($ub1, "a=b,b=d,b=e");
is($ub2, "a=b,a=c,b=d,b=e");
is($ub3, "a=b,a=c,b=d,b=e,c=f,c=g");

my $g2 = Graph->new;

for my $p (qw(zero
	      one
	      two
	      three
	      four
	      five
	      six
	      seven
	      eight
	      nine
	      ten)) {
    $g2->add_path(split(//, $p));
}

my ($gamma, %clustering) = $g2->clustering_coefficient;

my $eps = 1e-6;

ok(abs($gamma - 0.402222222222222) <= $eps);
ok(abs($clustering{e} - 0.7) <= $eps);
ok(abs($clustering{t} - 1/3) <= $eps);
is($clustering{z}, 0.0);
is($clustering{r}, 1.0);

my %betweenness = $g2->betweenness;

ok(abs($betweenness{e} - 60.3333333333333) <= $eps);
ok(abs($betweenness{t} - 17.1666666666667) <= $eps);
is($betweenness{x}, 0.0);
is($betweenness{u}, 3.0);
