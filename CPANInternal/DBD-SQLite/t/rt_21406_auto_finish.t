#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 11;
use Test::NoWarnings;

SCOPE: {
	my $dbh = connect_ok( RaiseError => 1 );
	$dbh->do("CREATE TABLE f (f1, f2, f3)");
	$dbh->do("INSERT INTO f VALUES (?, ?, ?)", {}, 'foo', 'bar', 1);
	$dbh->do("INSERT INTO f VALUES (?, ?, ?)", {}, 'foo', 'bar', 2);
	$dbh->do("INSERT INTO f VALUES (?, ?, ?)", {}, 'foo', 'bar', 3);
	$dbh->do("INSERT INTO f VALUES (?, ?, ?)", {}, 'foo', 'bar', 4);
	$dbh->do("INSERT INTO f VALUES (?, ?, ?)", {}, 'foo', 'bar', 5);

	my $sth1 = $dbh->prepare_cached('SELECT * FROM f ORDER BY f3', {});
	isa_ok( $sth1, 'DBI::st' );
	ok( $sth1->execute, '->execute ok' );
	is_deeply( $sth1->fetchrow_arrayref, [ 'foo', 'bar', 1 ], 'Row 1 ok' );
	is_deeply( $sth1->fetchrow_arrayref, [ 'foo', 'bar', 2 ], 'Row 2 ok' );

	my $sth2 = $dbh->prepare_cached('SELECT * FROM f ORDER BY f3', {}, 3);
	isa_ok( $sth2, 'DBI::st' );
	ok( $sth2->execute, '->execute ok' );
	is_deeply( $sth2->fetchrow_arrayref, [ 'foo', 'bar', 1 ], 'Row 1 ok' );
	is_deeply( $sth2->fetchrow_arrayref, [ 'foo', 'bar', 2 ], 'Row 2 ok' );
	ok( $sth2->finish, '->finish ok' );
}
