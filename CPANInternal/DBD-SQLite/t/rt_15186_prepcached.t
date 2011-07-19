#!/usr/bin/perl

# This is a regression test for bug #15186:
# http://rt.cpan.org/Public/Bug/Display.html?id=15186
# About re-using statements with prepare_cached().

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 13;
use Test::NoWarnings;

# Create a database
my $dbh = connect_ok( RaiseError => 1 );

# Create the table
ok( $dbh->do(<<'END_SQL'), 'CREATE TABLE' );
CREATE TABLE one (
    id INTEGER NOT NULL,
    name CHAR (64) NOT NULL
)
END_SQL

# Fill the table
ok(
	$dbh->do('INSERT INTO one values ( 1, ? )', {}, 'A'),
	'INSERT 1',
);
ok(
	$dbh->do('INSERT INTO one values ( 2987, ? )', {}, 'Not used'),
	'INSERT 1',
);
ok(
	$dbh->do('INSERT INTO one values ( 2, ? )', {}, 'Gary Shea'),
	'INSERT 1',
);

# Check that prepare_cached works
my $sql = "SELECT name FROM one WHERE id = ?";
SCOPE: {
	my $sth = $dbh->prepare_cached($sql);
	isa_ok( $sth, 'DBI::st' );
	is(
		($dbh->selectrow_array($sth, undef, 1))[0],
		'A',
		'Query 1 Row 1',
	);
}
SCOPE: {
	my $sth = $dbh->prepare_cached($sql);
	isa_ok( $sth, 'DBI::st' );
	is(
		($dbh->selectrow_array($sth, undef, 1))[0],
		'A',
		'Query 2 Row 1',
	);
	is(
		($dbh->selectrow_array($sth, undef, 2))[0],
		'Gary Shea',
		'Query 2 Row 2',
	);
}
SCOPE: {
	my $sth = $dbh->prepare_cached($sql);
	isa_ok( $sth, 'DBI::st' );
	is(
		($dbh->selectrow_array($sth, undef, 2))[0],
		'Gary Shea',
		'Query 2 Row 2',
	);
}
