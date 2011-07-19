#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 8;
use Test::NoWarnings;

my $dbh = connect_ok( RaiseError => 1 );
$dbh->do("CREATE TABLE f (f1, f2, f3)");
$dbh->do("INSERT INTO f VALUES (?, ?, ?)", {}, 'foo', 'bar', 1);

SCOPE: {
	my $sth = $dbh->prepare('SELECT f1 as "a.a", * FROM f', {});
	isa_ok( $sth, 'DBI::st' );
	ok( $sth->execute, '->execute ok' );
	my $row = $sth->fetchrow_hashref;
	is_deeply( $row, {
		'a.a' => 'foo',
		'f1'  => 'foo',
		'f2'  => 'bar',
		'f3'  => 1,
	}, 'Shortname row ok' );
}

$dbh->do("PRAGMA full_column_names = 1");
$dbh->do("PRAGMA short_column_names = 0");

SCOPE: {
	my $sth = $dbh->prepare('SELECT f1 as "a.a", * FROM f', {});
	isa_ok( $sth, 'DBI::st' );
	ok( $sth->execute, '->execute ok' );
	my $row = $sth->fetchrow_hashref;
	is_deeply( $row, {
		'a.a' => 'foo',
		'f.f1'  => 'foo',
		'f.f2'  => 'bar',
		'f.f3'  => 1,
	}, 'Shortname row ok' );
}
