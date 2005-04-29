#!perl -w

print "1..4\n";

use strict;
use URI;

my $u = URI->new("urn:oid");

$u->oid(1..10);

#print "$u\n";

print "not " unless $u eq "urn:oid:1.2.3.4.5.6.7.8.9.10";
print "ok 1\n";

print "not " unless $u->oid eq "1.2.3.4.5.6.7.8.9.10";
print "ok 2\n";

print "not " unless $u->scheme eq "urn" && $u->nid eq "oid";
print "ok 3\n";

print "not " unless $u->oid eq $u->nss;
print "ok 4\n";
