use Graph;

use strict;
local $^W = 1;

print "1..23\n";

my $g = Graph->new(compat02 => 1);

$g->add_edge('a', 'b');
$g->add_edge('b', 'c');
$g->add_edge('b', 'd');
$g->add_edge('a', 'e');

print "not " if $g->has_attribute;
print "ok 1\n";

print "not " if $g->has_attribute('a');
print "ok 2\n";

print "not " if $g->has_attribute('x');
print "ok 3\n";

print "not " if $g->has_attribute('a', 'b');
print "ok 4\n";

print "not " if $g->has_attribute('a', 'x');
print "ok 5\n";

print "not " if $g->has_attribute('x', 'a');
print "ok 6\n";

print "not " if $g->has_attribute('x', 'y');
print "ok 7\n";

$g->set_attribute('foo', 'bar');

print "not " unless $g->has_attribute('foo');
print "ok 8\n";

print "not " unless $g->get_attribute('foo') eq 'bar';
print "ok 9\n";

$g->set_attribute('foo', 'a', 'zog');

print "not " unless $g->has_attribute('foo', 'a');
print "ok 10\n";

print "not " unless $g->get_attribute('foo', 'a') eq 'zog';
print "ok 11\n";

$g->set_attribute('foo', 'a', 'b', 'zap');

print "not " unless $g->has_attribute('foo', 'a', 'b');
print "ok 12\n";

print "not " unless $g->get_attribute('foo', 'a', 'b') eq 'zap';
print "ok 13\n";

$g->set_attribute('goo', 'a', 'zik');

print "not " unless $g->has_attribute('goo', 'a');
print "ok 14\n";

print "not " unless $g->get_attribute('goo', 'a') eq 'zik';
print "ok 15\n";

my %a = $g->get_attributes('a');

print "not "
    unless scalar keys %a == 2 and $a{foo} eq 'zog' and $a{goo} eq 'zik';
print "ok 16\n";
$g->delete_attribute('foo', 'a');
%a = $g->get_attributes('a');
print "not "
    unless scalar keys %a == 1 and $a{goo} eq 'zik';
print "ok 17\n";

print "not " if $g->get_attribute('foo', 'a');
print "ok 18\n";

$g->delete_attributes('a');

%a = $g->get_attributes('a');

print "not "
    unless scalar keys %a == 0;
print "ok 19\n";

# The rest are not in 0.2xxx.

print "not "
    if $g->has_attributes('a');
print "ok 20\n";

$g->set_attributes('a', {'foo' => 42});

print "not "
    unless $g->has_attributes('a');
print "ok 21\n";

print "not "
    unless (%a = $g->get_attributes('a'));
print "ok 22\n";

print "not "
    unless $a{foo} == 42;
print "ok 23\n";
