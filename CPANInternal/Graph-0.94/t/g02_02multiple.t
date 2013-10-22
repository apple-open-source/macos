use Graph::Directed;
use Graph;

use strict;
local $^W = 1;

print "1..31\n";

my $g = Graph::Directed->new(compat02 => 1);

$g->add_vertices('a'..'j');

print "not " unless $g->vertices == 10;
print "ok 1\n";

print "not " unless join(" ", sort $g->vertices) eq "a b c d e f g h i j";
print "ok 2\n";

print "not " unless $g->edges == 0;
print "ok 3\n";

foreach ('f'..'z') { $g->delete_vertex($_) };

print "not " unless $g->vertices == 5;
print "ok 4\n";

print "not " unless join(" ", sort $g->vertices) eq "a b c d e";
print "ok 5\n";

$g->add_path('a', 'b', 'x', 'y', 'c');

print "not " unless $g->vertices == 7;
print "ok 6\n";

print "not " unless join(" ", sort $g->vertices) eq "a b c d e x y";
print "ok 7\n";

print "not " unless $g->edges == 4;
print "ok 8\n";

print "not " unless join(" ", $g->edges) eq "a b b x x y y c";
print "ok 9\n";

$g->delete_path('b', 'x', 'y', 'z');

print "not " unless $g->vertices == 7;
print "ok 10\n";

print "not " unless join(" ", sort $g->vertices) eq "a b c d e x y";
print "ok 11\n";

print "not " unless $g->edges == 2;
print "ok 12\n";

print "not " unless join(" ", $g->edges) eq "a b y c";
print "ok 13\n";

$g->add_cycle('x', 'y', 'z');

print "not " unless $g->vertices == 8;
print "ok 14\n";

print "not " unless join(" ", sort $g->vertices) eq "a b c d e x y z";
print "ok 15\n";

print "not " unless $g->edges == 5;
print "ok 16\n";

print "not " unless join(" ", $g->edges) eq "a b x y y c y z z x";
print "ok 17\n";

$g->delete_cycle('y', 'z', 'x');

print "not " unless $g->vertices == 8;
print "ok 18\n";

print "not " unless join(" ", sort $g->vertices) eq "a b c d e x y z";
print "ok 19\n";

print "not " unless $g->edges == 2;
print "ok 20\n";

print "not " unless join(" ", $g->edges) eq "a b y c";
print "ok 21\n";

$g->add_edges('p', 'q', 'r', 's');

print "not " unless $g->vertices == 12;
print "ok 22\n";

print "not " unless join(" ", sort $g->vertices) eq "a b c d e p q r s x y z";
print "ok 23\n";

print "not " unless $g->edges == 4;
print "ok 24\n";

print "not " unless join(" ", $g->edges) eq "a b p q r s y c";
print "ok 25\n";

$g->add_edges('p', 'q', 'r', 's');

print "not " unless $g->vertices == 12;
print "ok 26\n";

print "not " unless join(" ", $g->vertices) eq "a b c d e p q r s x y z";
print "ok 27\n";

print "not " unless $g->edges == 6;
print "ok 28\n";

print "not " unless join(" ", $g->edges) eq "a b p q p q r s r s y c";
print "ok 29\n";

my $f = Graph->new(compat02 => 1);

$f->add_edge('p', 'x');
$g->delete_edge('p', 'q');
$g->add_edge($f->edges);

print "not " unless $g->edges == 6;
print "ok 30\n";

print "not " unless join(" ", $g->edges) eq "a b p q p x r s r s y c";
print "ok 31\n";
