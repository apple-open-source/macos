#!/usr/bin/perl

# I've disabled warnings, so theoretically warnings shouldn't be printed

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 6;
use Test::NoWarnings;

SCOPE: {
	my $dbh = connect_ok( RaiseError => 1, PrintWarn => 0 );
	ok( ! $dbh->{PrintWarn}, '->{PrintWarn} is false' );
	ok( $dbh->do("CREATE TABLE f (f1, f2, f3)"), 'CREATE TABLE ok' );
	ok( $dbh->begin_work, '->begin_work' );
	ok(
		$dbh->do("INSERT INTO f VALUES (?, ?, ?)", {}, 'foo', 'bar', 1),
		'INSERT ok',
	);
}
