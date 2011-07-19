#!/usr/bin/perl

# This is a simple insert/fetch test.

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 10;
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

# Insert a row
ok( $dbh->do("INSERT INTO one VALUES ( 1, 'A' )"), 'INSERT' );

# Now SELECT the row out
is_deeply(
	$dbh->selectall_arrayref('SELECT * FROM one WHERE id = 1'),
	[ [ 1, 'A' ] ],
	'SELECT ok',
);

# Delete the row
ok( $dbh->do("DELETE FROM one WHERE id = 1"), 'DELETE' );

# Select an empty result
SCOPE: {
	my $sth = $dbh->prepare('SELECT * FROM one WHERE id = 1');
	isa_ok( $sth, 'DBI::st' );
	ok( $sth->execute, '->execute' );
	my $row1 = $sth->fetchrow_arrayref;
	is( $row1, undef, 'fetch select deleted' );
	my $row2 = $sth->fetchrow_arrayref;
	is( $row2, undef, 'fetch empty statement handler' );
}
