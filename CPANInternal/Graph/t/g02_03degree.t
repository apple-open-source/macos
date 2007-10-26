use Graph::Directed;

use strict;
local $^W = 1;

print "1..30\n";

my $g = Graph::Directed->new(compat02 => 1);

$g->add_edge('a', 'b');
$g->add_edge('b', 'c');
$g->add_edge('a', 'd');
$g->add_vertex('e');

print "not " unless $g->in_degree('a')  ==  0;
print "ok 1\n";

print "not " unless $g->out_degree('a') ==  2;
print "ok 2\n";

print "not " unless $g->degree('a')     == -2;
print "ok 3\n";

print "not " unless $g->in_degree('b')  ==  1;
print "ok 4\n";

print "not " unless $g->out_degree('b') ==  1;
print "ok 5\n";

print "not " unless $g->degree('b')     ==  0;
print "ok 6\n";

print "not " unless $g->in_degree('c')  ==  1;
print "ok 7\n";

print "not " unless $g->out_degree('c') ==  0;
print "ok 8\n";

print "not " unless $g->degree('c')     ==  1;
print "ok 9\n";

print "not " unless $g->in_degree('d')  ==  1;
print "ok 10\n";

print "not " unless $g->out_degree('d') ==  0;
print "ok 11\n";

print "not " unless $g->degree('d')     ==  1;
print "ok 12\n";

print "not " unless $g->in_degree('e')  ==  0;
print "ok 13\n";

print "not " unless $g->out_degree('e') ==  0;
print "ok 14\n";

print "not " unless $g->degree('e')     ==  0;
print "ok 15\n";

print "not " if defined $g->in_degree('x');
print "ok 16\n";

print "not " if defined $g->out_degree('x');
print "ok 17\n";

print "not " if defined $g->degree('x');
print "ok 18\n";

print "not " unless join(" ", $g->in_edges('b')) eq "a b";
print "ok 19\n";

print "not " unless join(" ", $g->out_edges('b')) eq "b c";
print "ok 20\n";

print "not " unless join(" ", $g->edges('b')) eq "a b b c" ||
                    join(" ", $g->edges('b')) eq "b c a b";
print "ok 21\n";

print "not " if defined $g->in_edges('x');
print "ok 22\n";

print "not " if defined $g->out_edges('x');
print "ok 23\n";

print "not " unless $g->edges == 3;
print "ok 24\n";

print "not " unless join(" ", sort $g->sink_vertices) eq "c d";
print "ok 25\n";

print "not " unless join(" ", sort $g->source_vertices) eq "a";
print "ok 26\n";

print "not " unless join(" ", sort $g->exterior_vertices) eq "a c d e";
print "ok 27\n";

print "not " unless join(" ", sort $g->interior_vertices) eq "b";
print "ok 28\n";

$g->add_cycle('f');

print "not " unless join(" ", sort $g->self_loop_vertices) eq "f";
print "ok 29\n";

print "not " unless join(" ", sort $g->interior_vertices) eq "b";
print "ok 30\n";

