use Test::More tests => 44;

use strict;
use Graph;

my $g = Graph::Undirected->new;

$g->add_edge("a", "b");
$g->add_edge("c", "d");

for (1..10) {
  my @v1 = $g->SP_Dijkstra("a", "c");
  is(@v1, 0);
  my @v2 = $g->SP_Dijkstra("a", "d");
  is(@v2, 0);
  my @v3 = $g->SP_Dijkstra("b", "c");
  is(@v3, 0);
  my @v4 = $g->SP_Dijkstra("b", "d");
  is(@v4, 0);
}

$g->add_edge("c", "b");

my @v1 = $g->SP_Dijkstra("a", "c");
is("@v1", "a b c");
my @v2 = $g->SP_Dijkstra("a", "d");
is("@v2", "a b c d");
my @v3 = $g->SP_Dijkstra("b", "c");
is("@v3", "b c");
my @v4 = $g->SP_Dijkstra("b", "d");
is("@v4", "b c d");
