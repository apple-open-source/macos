#!perl -w

print "1..18\n";

use strict;
use URI ();
my $u = URI->new("", "http");
my @q;

$u->query_form(a => 3, b => 4);

print "not " unless $u eq "?a=3&b=4";
print "ok 1\n";

$u->query_form(a => undef);
print "not " unless $u eq "?a=";
print "ok 2\n";

$u->query_form("a[=&+#] " => " [=&+#]");
print "not " unless $u eq "?a%5B%3D%26%2B%23%5D+=+%5B%3D%26%2B%23%5D";
print "ok 3\n";

@q = $u->query_form;
print "not " unless join(":", @q) eq "a[=&+#] : [=&+#]";
print "ok 4\n";

@q = $u->query_keywords;
print "not " if @q;
print "ok 5\n";

$u->query_keywords("a", "b");
print "not " unless $u eq "?a+b";
print "ok 6\n";

$u->query_keywords(" ", "+", "=", "[", "]");
print "not " unless $u eq "?%20+%2B+%3D+%5B+%5D";
print "ok 7\n";

@q = $u->query_keywords;
print "not " unless join(":", @q) eq " :+:=:[:]";
print "ok 8\n";

@q = $u->query_form;
print "not " if @q;
print "ok 9\n";

$u->query(" +?=#");
print "not " unless $u eq "?%20+?=%23";
print "ok 10\n";

$u->query_keywords([qw(a b)]);
print "not " unless $u eq "?a+b";
print "ok 11\n";

$u->query_keywords([]);
print "not " unless $u eq "";
print "ok 12\n";

$u->query_form({ a => 1, b => 2 });
print "not " unless $u eq "?a=1&b=2" || $u eq "?b=2&a=1";
print "ok 13\n";

$u->query_form([ a => 1, b => 2 ]);
print "not " unless $u eq "?a=1&b=2";
print "ok 14\n";

$u->query_form({});
print "not " unless $u eq "";
print "ok 15\n";

$u->query_form([a => [1..4]]);
print "not " unless $u eq "?a=1&a=2&a=3&a=4";
print "ok 16\n";

$u->query_form([]);
print "not " unless $u eq "";
print "ok 17\n";

$u->query_form(a => { foo => 1 });
print "not " unless $u =~ /^\?a=HASH\(/;
print "ok 18\n";

__END__
# Some debugging while writing new tests
print "\@q='", join(":", @q), "'\n";
print "\$u='$u'\n";

