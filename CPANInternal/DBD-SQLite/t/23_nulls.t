#!/usr/bin/perl

# This is a test for correctly handling NULL values.

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 9;

# Create a database
my $dbh = connect_ok();

# Create the table
ok( $dbh->do(<<'END_SQL'), 'CREATE TABLE' );
CREATE TABLE one (
    id INTEGER,
    name CHAR (64)
)
END_SQL

# Test whether or not a field containing a NULL is returned correctly
# as undef, or something much more bizarre.
ok(
	$dbh->do('INSERT INTO one VALUES ( NULL, ? )', {}, 'NULL-valued id' ),
	'INSERT',
);

SCOPE: {
	my $sth = $dbh->prepare('SELECT * FROM one WHERE id IS NULL');
	isa_ok( $sth, 'DBI::st' );
	ok( $sth->execute, '->execute ok' );
	my $row = $sth->fetchrow_arrayref;
	is( scalar(@$row), 2, 'Two values in the row' );
	is( $row->[0], undef, 'First column is undef' );
	is( $row->[1], 'NULL-valued id', 'Second column is defined' );
	ok( $sth->finish, '->finish' );
}
