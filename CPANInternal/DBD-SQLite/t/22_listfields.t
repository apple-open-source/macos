#!/usr/bin/perl

# This is a test for statement attributes being present appropriately.

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 12;
use Test::NoWarnings;

# Create a database
my $dbh = connect_ok();

# Create the table
ok( $dbh->do(<<'END_SQL'), 'CREATE TABLE' );
CREATE TABLE one (
    id INTEGER NOT NULL,
    name CHAR (64)
)
END_SQL

SCOPE: {
	# Create the statement
	my $sth = $dbh->prepare('SELECT * from one');
	isa_ok( $sth, 'DBI::st' );

	# Execute the statement
	ok( $sth->execute, '->execute' );

	# Check the field metadata
	is( $sth->{NUM_OF_FIELDS}, 2, 'Found 2 fields' );
	is_deeply( $sth->{NAME}, [ 'id', 'name' ], 'Names are ok' );
	ok( $sth->finish, '->finish ok' );
}

SCOPE: {
	# Check field metadata on a drop statement
	my $sth = $dbh->prepare('DROP TABLE one');
	isa_ok( $sth, 'DBI::st' );
	ok( $sth->execute, '->execute' );
	is( $sth->{NUM_OF_FIELDS}, 0, 'No fields in statement' );
	ok( $sth->finish, '->finish ok' );
}
