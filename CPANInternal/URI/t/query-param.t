#!perl -w

print "1..18\n";

use strict;

use URI;
use URI::QueryParam;

my $u = URI->new("http://www.sol.no?foo=4&bar=5&foo=5");

my $h = $u->query_form_hash;
print "not " unless $h->{foo}[0] eq "4" && $h->{foo}[1] eq "5" && $h->{bar} eq "5";
print "ok 1\n";

$u->query_form_hash({ a => 1, b => 2});
print "not " unless $u->query eq "a=1&b=2" || $u->query eq "b=2&a=1";
print "ok 2\n";

$u->query("a=1&b=2&a=3&b=4&a=5");
print "not " unless $u->query_param == 2 && join(":", $u->query_param) eq "a:b";
print "ok 3\n";

print "not " unless $u->query_param("a") eq "1" &&
                    join(":", $u->query_param("a")) eq "1:3:5";
print "ok 4\n";

print "not " unless $u->query_param(a => 11 .. 14) eq "1";
print "ok 5\n";

print "not " unless $u->query eq "a=11&b=2&a=12&b=4&a=13&a=14";
print "ok 6\n";

print "not " unless join(":", $u->query_param(a => 11)) eq "11:12:13:14";
print "ok 7\n";

print "not " unless $u->query eq "a=11&b=2&b=4";
print "ok 8\n";

print "not " unless $u->query_param_delete("a") eq "11";
print "ok 9\n";

print "not " unless $u->query eq "b=2&b=4";
print "ok 10\n";

$u->query_param_append(a => 1, 3, 5);
$u->query_param_append(b => 6);

print "not " unless $u->query eq "b=2&b=4&a=1&a=3&a=5&b=6";
print "ok 11\n";

$u->query_param(a => []);  # same as $u->query_param_delete("a");

print "not " unless $u->query eq "b=2&b=4&b=6";
print "ok 12\n";

$u->query(undef);
$u->query_param(a => 1, 2, 3);
$u->query_param(b => 1);

print "not " unless $u->query eq 'a=3&a=2&a=1&b=1';
print "ok 13\n";

$u->query_param_delete('a');
$u->query_param_delete('b');

print "not " if $u->query;
print "ok 14\n";

print "not " unless $u->as_string eq 'http://www.sol.no';
print "ok 15\n";

$u->query(undef);
$u->query_param(a => 1, 2, 3);
$u->query_param(b => 1);

print "not " unless $u->query eq 'a=3&a=2&a=1&b=1';
print "ok 16\n";

$u->query_param('a' => []);
$u->query_param('b' => []);

print "not " if $u->query;
print "ok 17\n";

print "not " unless $u->as_string eq 'http://www.sol.no';
print "ok 18\n";
