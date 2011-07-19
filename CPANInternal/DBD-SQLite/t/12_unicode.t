#!/usr/bin/perl

# This is a test for correct handling of the "unicode" database
# handle parameter.

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More;
BEGIN {
	if ( $] >= 5.008005 ) {
		plan( tests => 19 );
	} else {
		plan( skip_all => 'Unicode is not supported before 5.8.5' );
	}
}
use Test::NoWarnings;

#
#   Include std stuff
#
use Carp;
use DBI qw(:sql_types);

# Unintuitively, still has the effect of loading bytes.pm :-)
no bytes;

# Portable albeit kludgy: detects UTF-8 promotion of $hibyte from
# the abnormal length increase of $string concatenated to it.
sub is_utf8 {
	no bytes;
	my ($string) = @_;
	my $hibyte  = pack("C", 0xe9);
	my @lengths = map { bytes::length($_) } ($string, $string . $hibyte);
	return ($lengths[0] + 1 < $lengths[1]);
}

# First, some UTF-8 framework self-test:
my @isochars   = (ord("K"), 0xf6, ord("n"), ord("i"), ord("g"));
my $bytestring = pack("C*", @isochars);
my $utfstring  = pack("U*", @isochars);

ok(length($bytestring) == @isochars, 'Correct length for $bytestring');
ok(length($utfstring) == @isochars, 'Correct length for $utfstring');
ok(
	is_utf8($utfstring),
	'$utfstring should be marked as UTF-8 by Perl',
);
ok(
	! is_utf8($bytestring),
	'$bytestring should *NOT* be marked as UTF-8 by Perl',
);

# Sends $ain and $bin into TEXT resp. BLOB columns the database, then
# reads them again and returns the result as a list ($aout, $bout).
### Real DBD::SQLite testing starts here
my ($textback, $bytesback);
SCOPE: {
	my $dbh = connect_ok( dbfile => 'foo', RaiseError => 1 );
	is( $dbh->{sqlite_unicode}, 0, 'Unicode is off' );
	ok(
		$dbh->do("CREATE TABLE table1 (a TEXT, b BLOB)"),
		'CREATE TABLE',
	);

	($textback, $bytesback) = database_roundtrip($dbh, $bytestring, $bytestring);

	ok(
		! is_utf8($bytesback),
		"Reading blob gives binary",
	);
	ok(
		! is_utf8($textback),
		"Reading text gives binary too (for now)",
	);
	is($bytesback, $bytestring, "No blob corruption");
	is($textback, $bytestring, "Same text, different encoding");
}

# Start over but now activate Unicode support.
SCOPE: {
	my $dbh = connect_ok( dbfile => 'foo', sqlite_unicode => 1 );
	is( $dbh->{sqlite_unicode}, 1, 'Unicode is on' );

	($textback, $bytesback) = database_roundtrip($dbh, $utfstring, $bytestring);

	ok(! is_utf8($bytesback), "Reading blob still gives binary");
	ok(is_utf8($textback), "Reading text returns UTF-8");
	ok($bytesback eq $bytestring, "Still no blob corruption");
	ok($textback eq $utfstring, "Same text");

	my $lengths = $dbh->selectall_arrayref(
		"SELECT length(a), length(b) FROM table1"
	);

	ok(
		$lengths->[0]->[0] == $lengths->[0]->[1],
		"Database actually understands char set"
	)
	or
	warn "($lengths->[0]->[0] != $lengths->[0]->[1])";
}

sub database_roundtrip {
	my ($dbh, $ain, $bin) = @_;
	$dbh->do("DELETE FROM table1");
	my $sth = $dbh->prepare("INSERT INTO table1 (a, b) VALUES (?, ?)");
	$sth->bind_param(1, $ain, SQL_VARCHAR);
	$sth->bind_param(2, $bin, SQL_BLOB   );
	$sth->execute();
	$sth = $dbh->prepare("SELECT a, b FROM table1");
	$sth->execute();
	my @row = $sth->fetchrow_array;
	undef $sth;
	croak "Bad row length ".@row unless (@row == 2);
	@row;
}
