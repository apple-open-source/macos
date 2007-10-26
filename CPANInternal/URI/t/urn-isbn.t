#!perl -w

eval {
    require Business::ISBN;
};
if ($@) {
    print "1..0 # Skipped: Needs the Business::ISBN module installed\n\n";
    print $@;
    exit;
}

print "1..13\n";

use URI;
my $u = URI->new("URN:ISBN:0395363411");

print "not " unless $u eq "URN:ISBN:0395363411" &&
                    $u->scheme eq "urn" &&
                    $u->nid eq "isbn";
print "ok 1\n";

print "not " unless $u->canonical eq "urn:isbn:0-395-36341-1";
print "ok 2\n";

print "not " unless $u->isbn eq "0-395-36341-1";
print "ok 3\n";

print "not " unless $u->isbn_country_code == 0;
print "ok 4\n";

print "not " unless $u->isbn_publisher_code == 395;
print "ok 5\n";

print "not " unless $u->isbn_as_ean eq "9780395363416";
print "ok 6\n";

print "not " unless $u->nss eq "0395363411";
print "ok 7\n";

print "not " unless $u->isbn("0-88730-866-x") eq "0-395-36341-1";
print "ok 8\n";

print "not " unless $u->nss eq "0-88730-866-x";
print "ok 9\n";

print "not " unless $u->isbn eq "0-88730-866-X";
print "ok 10\n";

print "not " unless URI::eq("urn:isbn:088730866x", "URN:ISBN:0-88-73-08-66-X");
print "ok 11\n";

# try to illegal ones
$u = URI->new("urn:ISBN:abc");
print "not " unless $u eq "urn:ISBN:abc";
print "ok 12\n";

print "not " if $u->nss ne "abc" || defined $u->isbn;
print "ok 13\n";



