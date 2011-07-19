#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test qw/connect_ok @CALL_FUNCS/;
use Test::More;
BEGIN {
	if ( $] >= 5.008005 ) {
		plan( tests => 15 * @CALL_FUNCS + 1);
	} else {
		plan( skip_all => 'Unicode is not supported before 5.8.5' );
	}
}
use Test::NoWarnings;

eval "require utf8";
die $@ if $@;

foreach my $call_func (@CALL_FUNCS) {
	my $dbh = connect_ok( sqlite_unicode => 1 );
	ok($dbh->$call_func( "perl_uc", 1, \&perl_uc, "create_function" ));

	ok( $dbh->do(<<'END_SQL'), 'CREATE TABLE' );
CREATE TABLE foo (
	bar varchar(255)
)
END_SQL

	my @words = qw{Bergère hôte hétaïre hêtre};
	foreach my $word (@words) {
		utf8::upgrade($word);
		ok( $dbh->do("INSERT INTO foo VALUES ( ? )", {}, $word), 'INSERT' );
		my $foo = $dbh->selectall_arrayref("SELECT perl_uc(bar) FROM foo");
		is_deeply( $foo, [ [ perl_uc($word) ] ], 'unicode upcase ok' );
		ok( $dbh->do("DELETE FROM foo"), 'DELETE ok' );
	}
	$dbh->disconnect;
}

sub perl_uc {
	my $string = shift;
	return uc($string);
}
