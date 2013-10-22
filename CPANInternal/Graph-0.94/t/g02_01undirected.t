use Graph::Undirected;

use strict;
local $^W = 1;

print "1..34\n";

my $u = Graph::Undirected->new(compat02 => 1);

print "not " unless $u eq "";
print "ok 1\n";

print "not " unless $u->vertices == 0;
print "ok 2\n";

print "not " unless $u->edges    == 0;
print "ok 3\n";

print "not " unless $u->density  == 0;
print "ok 4\n";

print "not " unless $u->average_degree == 0;
print "ok 5\n";

print "not " if defined $u->vertex('a');
print "ok 6\n";

print "not " if $u->edges('a');
print "ok 7\n";

$u->add_edge('a', 'b');
$u->add_edge('a', 'c');
$u->add_edge('c', 'd');
$u->add_vertex('e');

print "not " unless $u eq "a=b,a=c,c=d,e";
print "ok 8\n";

print "not " unless $u->vertices == 5;
print "ok 9\n";

print "not " unless join(" ", sort $u->vertices) eq "a b c d e";
print "ok 10\n";

print "not " unless $u->edges    == 3;
print "ok 11\n";

print "not " unless $u->density  == 3/10;
print "ok 12\n";

print "not " unless join(" ", map { int } $u->density_limits) eq "2 7 10";
print "ok 13\n";

print "not " unless $u->average_degree == 6/5;
print "ok 14\n";

print "not " unless defined $u->vertex('a');
print "ok 15\n";

print "not " if defined $u->vertex('x');
print "ok 16\n";

print "not " unless $u->edges('a') == 2;
print "ok 17\n";

print "not "
	unless join(" ", $u->edges('a')) eq "a b a c" ||
               join(" ", $u->edges('a')) eq "a c a b";
print "ok 18\n";

print "not " unless $u->successors('a') == 2;
print "ok 19\n";

print "not " unless join(" ", sort $u->successors('a')) eq "b c";
print "ok 20\n";

print "not " unless $u->successors('b') == 1;
print "ok 21\n";

print "not " unless join(" ", sort $u->successors('b')) eq "a";
print "ok 22\n";

print "not " unless $u->predecessors('b') == 1;
print "ok 23\n";

print "not " unless join(" ", sort $u->predecessors('b')) eq "a";
print "ok 24\n";

print "not " unless $u->predecessors('a') == 2;
print "ok 25\n";

print "not " unless join(" ", sort $u->predecessors('a')) eq "b c";
print "ok 26\n";

print "not " unless $u->neighbours('c') == 2;
print "ok 27\n";

print "not " unless join(" ", sort $u->neighbours('c')) eq "a d";
print "ok 28\n";

print "not " unless $u->neighbors('a') == 2;
print "ok 29\n";

print "not " unless join(" ", sort $u->neighbors('a')) eq "b c";
print "ok 30\n";

print "not " if $u->directed;
print "ok 31\n";

print "not " unless $u->undirected;
print "ok 32\n";

my $v = new Graph::Undirected;

$v->add_edge('1', '2');

print "not " unless $v->has_edge('1', '2');
print "ok 33\n";

print "not " unless $v->has_edge('2', '1');
print "ok 34\n";
