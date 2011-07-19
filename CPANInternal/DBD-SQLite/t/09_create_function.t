#!/usr/bin/perl

use 5.00503;
use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test qw/connect_ok @CALL_FUNCS/;
use Test::More;
use Test::NoWarnings;

plan tests => 27 * @CALL_FUNCS + 1;

sub now {
    return time();
}

sub add2 {
    my ( $a, $b ) = @_;
    return $a + $b;
}

sub my_sum {
    my $sum = 0;
    foreach my $x (@_) {
        $sum += $x;
    }
    return $sum;
}

sub error {
    die "function is dying: ", @_, "\n";
}

sub void_return {
}

sub return2 {
        return ( 1, 2 );
}

sub return_null {
        return undef;
}

sub my_defined {
        defined($_[0]) ? 1 : 0;
}

sub noop {
        return $_[0];
}

foreach my $call_func (@CALL_FUNCS) {
	my $dbh = connect_ok( PrintError => 0 );

	ok($dbh->$call_func( "now", 0, \&now, "create_function" ));
	my $result = $dbh->selectrow_arrayref( "SELECT now()" );

	ok( $result->[0], 'Got a result' );

	$dbh->do( 'CREATE TEMP TABLE func_test ( a, b )' );
	$dbh->do( 'INSERT INTO func_test VALUES ( 1, 3 )' );
	$dbh->do( 'INSERT INTO func_test VALUES ( 0, 4 )' );

	ok($dbh->$call_func( "add2", 2, \&add2, "create_function" ));
	$result = $dbh->selectrow_arrayref( "SELECT add2(1,3)" );
	is($result->[0], 4, "SELECT add2(1,3)" );

	$result = $dbh->selectall_arrayref( "SELECT add2(a,b) FROM func_test" );
	is_deeply( $result, [ [4], [4] ], "SELECT add2(a,b) FROM func_test" );

	ok($dbh->$call_func( "my_sum", -1, \&my_sum, "create_function" ));
	$result = $dbh->selectrow_arrayref( "SELECT my_sum( '2', 3, 4, '5')" );
	is( $result->[0], 14, "SELECT my_sum( '2', 3, 4, '5')" );

	ok($dbh->$call_func( "error", -1, \&error, "create_function" ));
	$result = $dbh->selectrow_arrayref( "SELECT error( 'I died' )" );
	ok( !$result );
	like( $DBI::errstr, qr/function is dying: I died/ );

	ok($dbh->$call_func( "void_return", -1, \&void_return, "create_function" ));
	$result = $dbh->selectrow_arrayref( "SELECT void_return( 'I died' )" );
	is_deeply( $result, [ undef ], "SELECT void_return( 'I died' )" );

	ok($dbh->$call_func( "return_null", -1, \&return_null, "create_function" ));
	$result = $dbh->selectrow_arrayref( "SELECT return_null()" );
	is_deeply( $result, [ undef ], "SELECT return_null()" );

	ok($dbh->$call_func( "return2", -1, \&return2, "create_function" ));
	$result = $dbh->selectrow_arrayref( "SELECT return2()" );
	is_deeply( $result, [ 2 ], "SELECT return2()" );

	ok($dbh->$call_func( "my_defined", 1, \&my_defined, "create_function" ));
	$result = $dbh->selectrow_arrayref( "SELECT my_defined(1)" );
	is_deeply( $result, [ 1 ], "SELECT my_defined(1)" );

	$result = $dbh->selectrow_arrayref( "SELECT my_defined('')" );
	is_deeply( $result, [ 1 ], "SELECT my_defined('')" );

	$result = $dbh->selectrow_arrayref( "SELECT my_defined('abc')" );
	is_deeply( $result, [ 1 ], "SELECT my_defined('abc')" );

	$result = $dbh->selectrow_arrayref( "SELECT my_defined(NULL)" );
	is_deeply( $result, [ '0' ], "SELECT my_defined(NULL)" );

	ok($dbh->$call_func( "noop", 1, \&noop, "create_function" ));
	$result = $dbh->selectrow_arrayref( "SELECT noop(NULL)" );
	is_deeply( $result, [ undef ], "SELECT noop(NULL)" );

	$result = $dbh->selectrow_arrayref( "SELECT noop(1)" );
	is_deeply( $result, [ 1 ], "SELECT noop(1)" );

	$result = $dbh->selectrow_arrayref( "SELECT noop('')" );
	is_deeply( $result, [ '' ], "SELECT noop('')" );

	$result = $dbh->selectrow_arrayref( "SELECT noop(1.0625)" );
	is_deeply( $result, [ 1.0625 ], "SELECT noop(1.0625)" );

	$dbh->disconnect;
}
