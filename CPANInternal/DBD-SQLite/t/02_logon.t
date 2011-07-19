#!/usr/bin/perl

# Tests basic login and pragma setting

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test qw/connect_ok @CALL_FUNCS/;
use Test::More;
use Test::NoWarnings;

plan tests => 9 * @CALL_FUNCS + 1;

my $show_diag = 0;
foreach my $call_func (@CALL_FUNCS) {

	# Ordinary connect
	SCOPE: {
		my $dbh = connect_ok();
		ok( $dbh->{sqlite_version}, '->{sqlite_version} ok' );
		is( $dbh->{AutoCommit}, 1, 'AutoCommit is on by default' );
		diag("sqlite_version=$dbh->{sqlite_version}") unless $show_diag++;
		ok( $dbh->$call_func('busy_timeout'), 'Found initial busy_timeout' );
		ok( $dbh->$call_func(5000, 'busy_timeout') );
		is( $dbh->$call_func('busy_timeout'), 5000, 'Set busy_timeout to new value' );
	}

	# Attributes in the connect string
	SKIP: {
		unless ( $] >= 5.008005 ) {
			skip( 'Unicode is not supported before 5.8.5', 2 );
		}
		my $dbh = DBI->connect( 'dbi:SQLite:dbname=foo;sqlite_unicode=1', '', '' );
		isa_ok( $dbh, 'DBI::db' );
		is( $dbh->{sqlite_unicode}, 1, 'Unicode is on' );
	}

	# Connect to a memory database
	SCOPE: {
		my $dbh = DBI->connect( 'dbi:SQLite:dbname=:memory:', '', '' );
		isa_ok( $dbh, 'DBI::db' );	
	}
}
