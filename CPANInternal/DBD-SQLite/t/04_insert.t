#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 14;
use Test::NoWarnings;

my $dbh = connect_ok();

ok( $dbh->do("CREATE TABLE f (f1, f2, f3)"), 'CREATE TABLE f' );
ok( $dbh->do("delete from f"), 'DELETE FROM f' );

SCOPE: {
	my $sth = $dbh->prepare("INSERT INTO f VALUES (?, ?, ?)", { go_last_insert_id_args => [undef, undef, undef, undef] });
	isa_ok($sth, 'DBI::st');
	my $rows = $sth->execute("Fred", "Bloggs", "fred\@bloggs.com");
	is( $rows, 1, '->execute returns 1 row' );

	is( $sth->execute("test", "test", "1"), 1 );
	is( $sth->execute("test", "test", "2"), 1 );
	is( $sth->execute("test", "test", "3"), 1 );

	SKIP: {
    		skip( 'last_insert_id requires DBI v1.43', 2 ) if $DBI::VERSION < 1.43;
    		is( $dbh->last_insert_id(undef, undef, undef, undef), 4 );
    		is( $dbh->func('last_insert_rowid'), 4, 'last_insert_rowid should be 4' );
	}

	SKIP: {
    		skip( 'method installation requires DBI v1.608', 2 ) if $DBI::VERSION < 1.608;
			can_ok($dbh, 'sqlite_last_insert_rowid');
    		is( $dbh->sqlite_last_insert_rowid, 4, 'last_insert_rowid should be 4' );
	}
}

is( $dbh->do("delete from f where f1='test'"), 3 );
