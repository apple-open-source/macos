#!/usr/bin/perl

# This tests, whether the number of rows can be retrieved.

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 18;
use Test::NoWarnings;

sub rows {
    my $sth      = shift;
    my $expected = shift;
    my $count    = 0;
    while ($sth->fetchrow_arrayref) {
	++$count;
    }
    Test::More::is( $count, $expected, "Got $expected rows" );
}

# Create a database
my $dbh = connect_ok();

# Create the table
ok( $dbh->do(<<'END_SQL'), 'CREATE TABLE' );
CREATE TABLE one (
    id INTEGER NOT NULL,
    name CHAR (64) NOT NULL
)
END_SQL

# Insert into table
ok(
	$dbh->do("INSERT INTO one VALUES ( 1, 'A' )"),
	'INSERT 1',
);

# Count the rows
SCOPE: {
	my $sth = $dbh->prepare('SELECT * FROM one WHERE id = 1');
	isa_ok( $sth, 'DBI::st' );
	ok( $sth->execute, '->execute' );
	rows( $sth, 1 );
	ok( $sth->finish, '->finish' );
}

# Insert another row
ok(
	$dbh->do("INSERT INTO one VALUES ( 2, 'Jochen Wiedmann' )"),
	'INSERT 2',
);

# Count the rows
SCOPE: {
	my $sth = $dbh->prepare('SELECT * FROM one WHERE id >= 1');
	isa_ok( $sth, 'DBI::st' );
	ok( $sth->execute, '->execute' );
	rows( $sth, 2 );
	ok( $sth->finish, '->finish' );
}

# Insert another row
ok(
	$dbh->do("INSERT INTO one VALUES ( 3, 'Tim Bunce' )"),
	'INSERT 3',
);

# Count the rows
SCOPE: {
	my $sth = $dbh->prepare('SELECT * FROM one WHERE id >= 2');
	isa_ok( $sth, 'DBI::st' );
	ok( $sth->execute, '->execute' );
	rows( $sth, 2 );
	ok( $sth->finish, '->finish' );
}
