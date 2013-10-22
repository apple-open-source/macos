use Graph;

use strict;
local $^W = 1;

print "1..10\n";

my $g = Graph->new(compat02 => 1);

$g->add_edge('a', 'b');
$g->add_edge('b', 'c');
$g->add_edge('a', 'd');
$g->add_vertex('e');

my $h = $g->transpose;

print "not " unless $g->vertices == $h->vertices;
print "ok 1\n";

print "not " unless join(" ", sort $h->vertices) eq "a b c d e";
print "ok 2\n";

print "not " unless $g->edges == $h->edges;
print "ok 3\n";

print "not " unless join(" ", $h->edges) eq "b a c b d a";
print "ok 4\n";

$g->delete_vertex('e');

print "not " unless join(" ", $h->vertices) eq "a b c d e";
print "ok 5\n";

print "not " unless join(" ", $h->edges) eq "b a c b d a";
print "ok 6\n";

$g->delete_vertex('b');

print "not " unless join(" ", $h->vertices) eq "a b c d e";
print "ok 7\n";

print "not " unless join(" ", $h->edges) eq "b a c b d a";
print "ok 8\n";

$g->delete_edge('a', 'b');

print "not " unless join(" ", $h->vertices) eq "a b c d e";
print "ok 9\n";

print "not " unless join(" ", $h->edges) eq "b a c b d a";
print "ok 10\n";
