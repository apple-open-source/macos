use Graph;

use strict;
local $^W = 1;

print "1..8\n";

my $g = Graph->new(compat02 => 1);

$g->add_edge('a', 'b');
$g->add_edge('b', 'c');

my $h = $g->complement;

print "not " unless $h->edges == $h->vertices * ($h->vertices - 1) - $g->edges;
print "ok 1\n";

print "not " unless join(" ", $h->vertices) eq "a b c";
print "ok 2\n";

print "not " unless $h->vertices == $g->vertices;
print "ok 3\n";

print "not " unless join(" ", $h->edges) eq "a c b a c a c b";
print "ok 4\n";

$g->delete_vertex('c');

print "not " unless join(" ", $h->vertices) eq "a b c";
print "ok 5\n";

print "not " unless join(" ", $h->edges) eq "a c b a c a c b";
print "ok 6\n";

$g->delete_edge('a', 'b');

print "not " unless join(" ", $h->vertices) eq "a b c";
print "ok 7\n";

print "not " unless join(" ", $h->edges) eq "a c b a c a c b";
print "ok 8\n";
