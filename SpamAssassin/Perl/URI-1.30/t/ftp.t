#!perl -w

print "1..13\n";

use strict;
use URI;
my $uri;

$uri = URI->new("ftp://ftp.example.com/path");

print "not " unless $uri->scheme eq "ftp";
print "ok 1\n";

print "not " unless $uri->host eq "ftp.example.com";
print "ok 2\n";

print "not " unless $uri->port eq 21;
print "ok 3\n";

print "not " unless $uri->user eq "anonymous";
print "ok 4\n";

print "not " unless $uri->password eq 'anonymous@';
print "ok 5\n";

$uri->userinfo("gisle\@aas.no");

print "not " unless $uri eq "ftp://gisle%40aas.no\@ftp.example.com/path";
print "ok 6\n";

print "not " unless $uri->user eq "gisle\@aas.no";
print "ok 7\n";

print "not " if defined($uri->password);
print "ok 8\n";

$uri->password("secret");

print "not " unless $uri eq "ftp://gisle%40aas.no:secret\@ftp.example.com/path";
print "ok 9\n";

$uri = URI->new("ftp://gisle\@aas.no:secret\@ftp.example.com/path");
print "not " unless $uri eq "ftp://gisle\@aas.no:secret\@ftp.example.com/path";
print "ok 10\n";

print "not " unless $uri->userinfo eq "gisle\@aas.no:secret";
print "ok 11\n";

print "not " unless $uri->user eq "gisle\@aas.no";
print "ok 12\n";

print "not " unless $uri->password eq "secret";
print "ok 13\n";
