#!/usr/bin/perl

# Check whether 'ChopBlanks' works.

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 14;
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
	$dbh->do('INSERT INTO one values ( 1, ? )', {}, 'NULL' ),
	'INSERT 1',
);
ok(
	$dbh->do('INSERT INTO one values ( 2, ? )', {}, ' '),
	'INSERT 2',
);
ok(
	$dbh->do('INSERT INTO one values ( 3, ? )', {}, ' a b c '),
	'INSERT 3',
);

# Test fetching with ChopBlanks off
SCOPE: {
	my $sth = $dbh->prepare('SELECT * FROM one ORDER BY id');
	isa_ok( $sth, 'DBI::st' );
	ok( $sth->execute, '->execute ok' );
	$sth->{ChopBlanks} = 0;
	my $rows = $sth->fetchall_arrayref;
	is_deeply( $rows, [
		[ 1, 'NULL'    ],
		[ 2, ' '       ],
		[ 3, ' a b c ' ],
	], 'ChopBlanks = 0' );
	ok( $sth->finish, '->finish' );
}

# Test fetching with ChopBlanks on
SCOPE: {
	my $sth = $dbh->prepare('SELECT * FROM one ORDER BY id');
	isa_ok( $sth, 'DBI::st' );
	ok( $sth->execute, '->execute ok' );
	$sth->{ChopBlanks} = 1;
	my $rows = $sth->fetchall_arrayref;
	is_deeply( $rows, [
		[ 1, 'NULL'   ],
		[ 2, ''       ],
		[ 3, ' a b c' ],
	], 'ChopBlanks = 1' );
	ok( $sth->finish, '->finish' );
}
