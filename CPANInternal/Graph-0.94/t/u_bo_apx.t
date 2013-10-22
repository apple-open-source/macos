use Test::More tests => 5;

use Graph;

use strict;

my $g = Graph::Undirected->new;

$g->add_edge(qw(a b));
$g->add_edge(qw(a c));
$g->add_edge(qw(b c));
$g->add_edge(qw(b d));
$g->add_edge(qw(d e));
$g->add_edge(qw(d f));
$g->add_edge(qw(e f));

my @a1 = sort $g->articulation_points();

is("@a1", "b d");

$g->add_edge(qw(b b));

my @a2 = sort $g->articulation_points();

is("@a2", "b d");

$g->add_edge(qw(d d));

my @a3 = sort $g->articulation_points();

is("@a3", "b d");

$g->add_edge(qw(a a));

my @a4 = sort $g->articulation_points();

is("@a4", "b d");

$g->add_edge(qw(f f));

my @a5 = sort $g->articulation_points();

is("@a5", "b d");

