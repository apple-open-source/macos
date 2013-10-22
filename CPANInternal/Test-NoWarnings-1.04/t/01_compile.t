#!/usr/bin/perl

use strict;
BEGIN {
	$| = 1;
	$^W = 1;
}

use Test::More tests => 4;

require_ok( 'Test::NoWarnings' );
ok( $Test::NoWarnings::VERSION, 'Loaded Test::NoWarnings' );
ok( $Test::NoWarnings::Warning::VERSION, 'Loaded Test::NoWarnings::Warning' );
is(
	$Test::NoWarnings::VERSION,
	$Test::NoWarnings::Warning::VERSION,
	'Loaded matching Test::NoWarnings::Warning',
);
