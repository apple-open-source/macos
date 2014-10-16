use Graph::Directed;

use strict;
local $^W = 1;

print "1..34\n";

my $g = Graph::Directed->new(compat02 => 1);

print "not " unless $g eq "";
print "ok 1\n";

print "not " unless $g->vertices == 0;
print "ok 2\n";

print "not " unless $g->edges    == 0;
print "ok 3\n";

print "not " unless $g->density  == 0;
print "ok 4\n";

print "not " unless $g->average_degree == 0;
print "ok 5\n";

print "not " if defined $g->vertex('a');
print "ok 6\n";

print "not " if $g->edges('a');
print "ok 7\n";

$g->add_edge('a', 'b');
$g->add_edge('a', 'c');
$g->add_edge('c', 'd');
$g->add_vertex('e');

print "not " unless $g eq "a-b,a-c,c-d,e";
print "ok 8 # $g\n";

print "not " unless $g->vertices == 5;
print "ok 9\n";

print "not " unless join(" ", sort $g->vertices) eq "a b c d e";
print "ok 10\n";

print "not " unless $g->edges    == 3;
print "ok 11\n";

print "not " unless $g->density  == 3/20;
print "ok 12\n";

print "not " unless join(" ", map { int } $g->density_limits) eq "5 15 20";
print "ok 13\n";
# print join(" ", map { int } $g->density_limits), "\n";

print "not " unless $g->average_degree == 0;
print "ok 14\n";

print "not " unless defined $g->vertex('a');
print "ok 15\n";

print "not " if defined $g->vertex('x');
print "ok 16\n";

print "not " unless $g->edges('a') == 2;
print "ok 17\n";

print "not "
	unless join(" ", $g->edges('a')) eq "a b a c" ||
               join(" ", $g->edges('a')) eq "a c a b";
print "ok 18\n";

print "not " unless $g->successors('a') == 2;
print "ok 19\n";

print "not " unless join(" ", sort $g->successors('a')) eq "b c";
print "ok 20\n";

print "not " unless $g->successors('b') == 0;
print "ok 21\n";

print "not " unless join(" ", sort $g->successors('b')) eq "";
print "ok 22\n";

print "not " unless $g->predecessors('b') == 1;
print "ok 23\n";

print "not " unless join(" ", sort $g->predecessors('b')) eq "a";
print "ok 24\n";

print "not " unless $g->predecessors('a') == 0;
print "ok 25\n";

print "not " unless join(" ", sort $g->predecessors('a')) eq "";
print "ok 26\n";

print "not " unless $g->neighbours('c') == 2;
print "ok 27\n";

print "not " unless join(" ", sort $g->neighbours('c')) eq "a d";
print "ok 28\n";

print "not " unless $g->neighbors('a') == 2;
print "ok 29\n";

print "not " unless join(" ", sort $g->neighbors('a')) eq "b c";
print "ok 30\n";

print "not " unless $g->directed;
print "ok 31\n";

print "not " if $g->undirected;
print "ok 32\n";

use Graph;

my $h = new Graph;

$h->add_edge('1', '2');

print "not " unless $h->has_edge('1', '2');
print "ok 33\n";

print "not " if $h->has_edge('2', '1');
print "ok 34\n";
