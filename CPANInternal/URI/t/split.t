#!perl -w

print "1..17\n";

use strict;
use URI::Split qw(uri_split uri_join);

sub j { join("-", map { defined($_) ? $_ : "<undef>" } @_) }

print "not " unless j(uri_split("p")) eq "<undef>-<undef>-p-<undef>-<undef>";
print "ok 1\n";

print "not " unless j(uri_split("p?q")) eq "<undef>-<undef>-p-q-<undef>";
print "ok 2\n";

print "not " unless j(uri_split("p#f")) eq "<undef>-<undef>-p-<undef>-f";
print "ok 3\n";

print "not " unless j(uri_split("p?q/#f/?")) eq "<undef>-<undef>-p-q/-f/?";
print "ok 4\n";

print "not " unless j(uri_split("s://a/p?q#f")) eq "s-a-/p-q-f";
print "ok 5\n";

print "not " unless uri_join("s", "a", "/p", "q", "f") eq "s://a/p?q#f";
print "ok 6\n";

print "not " unless uri_join("s", "a", "p", "q", "f") eq "s://a/p?q#f";
print "ok 7\n";

print "not " unless uri_join(undef, undef, "", undef, undef) eq "";
print "ok 8\n";

print "not " unless uri_join(undef, undef, "p", undef, undef) eq "p";
print "ok 9\n";

print "not " unless uri_join("s", undef, "p") eq "s:p";
print "ok 10\n";

print "not " unless uri_join("s") eq "s:";
print "ok 11\n";

print "not " unless uri_join() eq "";
print "ok 12\n";

print "not " unless uri_join("s", "a") eq "s://a";
print "ok 13\n";

print "not " unless uri_join("s", "a/b") eq "s://a%2Fb";
print "ok 14\n";

print "not " unless uri_join("s", ":/?#", ":/?#", ":/?#", ":/?#") eq "s://:%2F%3F%23/:/%3F%23?:/?%23#:/?#";
print "ok 15\n";

print "not " unless uri_join(undef, undef, "a:b") eq "a%3Ab";
print "ok 16\n";

print "not " unless uri_join("s", undef, "//foo//bar") eq "s:////foo//bar";
print "ok 17\n";
