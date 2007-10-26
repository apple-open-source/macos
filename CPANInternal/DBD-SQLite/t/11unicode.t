#!/usr/local/bin/perl
#
#   $Id: 11unicode.t,v 1.2 2007/01/30 07:01:06 wkakes Exp $
#
#   This is a test for correct handling of the "unicode" database
#   handle parameter.
#

$^W = 1;
use strict;

#
#   Include std stuff
#

use Carp;
use DBI qw(:sql_types);
our ($mdriver, $test_dsn, $test_user, $test_password, $file);
foreach $file ("lib.pl", "t/lib.pl") {
    do $file; if ($@) { print STDERR "Error while executing lib.pl: $@\n";
			   exit 10;
		      }
    last if ($mdriver);
}

BEGIN {if ($] < 5.006) {
    print <<"BAIL_OUT";
1..0
# SKIPPING - No UTF-8 support in this Perl release
BAIL_OUT
    exit 0;
}}

no bytes; # Unintuitively, still has the effect of loading bytes.pm :-)

# Portable albeit kludgy: detects UTF-8 promotion of $hibyte from
# the abnormal length increase of $string concatenated to it.
sub is_utf8 {
    no bytes;
    my ($string) = @_;
    my $hibyte = pack("C", 0xe9);

    my @lengths = map { bytes::length($_) } ($string, $string . $hibyte);
    return ($lengths[0] + 1 < $lengths[1]);
}

### Test code starts here

Testing(); our $numTests; $numTests = 14; Testing();

# First, some UTF-8 framework self-test:

my @isochars = (ord("K"), 0xf6, ord("n"), ord("i"), ord("g"));

my $bytestring = pack("C*", @isochars);
my $utfstring = pack("U*", @isochars);

Test(length($bytestring) == @isochars, 'Correct length for $bytestring');
Test(length($utfstring) == @isochars, 'Correct length for $utfstring');
Test(is_utf8($utfstring),
     '$utfstring should be marked as UTF-8 by Perl');
Test(! is_utf8($bytestring),
     '$bytestring should *NOT* be marked as UTF-8 by Perl');

### Real DBD::SQLite testing starts here

my $dbh = DBI->connect($test_dsn, $test_user, $test_password,
                       {RaiseError => 1})
	or die <<'MESSAGE';
Cannot connect to database $test_dsn, please check directory and
permissions.
MESSAGE

Test( (my $table = FindNewTable($dbh)), "FindNewTable")
	or DbiError($dbh->error, $dbh->errstr);

eval { $dbh->do("DROP TABLE $table"); };

$dbh->do("CREATE TABLE $table (a TEXT, b BLOB)");

# Sends $ain and $bin into TEXT resp. BLOB columns the database, then
# reads them again and returns the result as a list ($aout, $bout).
sub database_roundtrip {
    my ($ain, $bin) = @_;
    $dbh->do("DELETE FROM $table");
    my $sth = $dbh->prepare("INSERT INTO $table (a, b) VALUES (?, ?)");
    $sth->bind_param(1, $ain, SQL_VARCHAR);
    $sth->bind_param(2, $bin, SQL_BLOB);
    $sth->execute();

    $sth = $dbh->prepare("SELECT a, b FROM $table");
    $sth->execute();
    my @row = $sth->fetchrow_array;
    undef $sth;
    croak "Bad row length ".@row unless (@row == 2);
    @row;
}

my ($textback, $bytesback) =
    database_roundtrip($bytestring, $bytestring);

Test(! is_utf8($bytesback), "Reading blob gives binary");
Test(! is_utf8($textback), "Reading text gives binary too (for now)");
Test($bytesback eq $bytestring, "No blob corruption");
Test($textback eq $bytestring, "Same text, different encoding");

# Start over but now activate Unicode support.

$dbh->{unicode} = 1;

($textback, $bytesback) =
    database_roundtrip($utfstring, $bytestring);

Test(! is_utf8($bytesback), "Reading blob still gives binary");
Test(is_utf8($textback), "Reading text returns UTF-8");
Test($bytesback eq $bytestring, "Still no blob corruption");
Test($textback eq $utfstring, "Same text");

my $lengths = $dbh->selectall_arrayref
    ("SELECT length(a), length(b) FROM $table");

Test($lengths->[0]->[0] == $lengths->[0]->[1],
     "Database actually understands char set") or
    warn "($lengths->[0]->[0] != $lengths->[0]->[1])";

END { $dbh->do("DROP TABLE $table"); }
