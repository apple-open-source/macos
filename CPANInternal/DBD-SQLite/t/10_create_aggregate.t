#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test qw/connect_ok @CALL_FUNCS/;
use Test::More;
use Test::NoWarnings;

plan tests => 21 * @CALL_FUNCS + 1;

# Create the aggregate test packages
SCOPE: {
	package count_aggr;

	sub new {
		bless { count => 0 }, shift;
	}

	sub step {
		$_[0]{count}++;
		return;
	}

	sub finalize {
		my $c = $_[0]{count};
		$_[0]{count} = undef;

		return $c;
	}

	package obj_aggregate;

	sub new {
		bless { count => 0 }, shift;
	}

	sub step {
		$_[0]{count}++ if defined $_[1];
	}

	sub finalize {
		my $c = $_[0]{count};
		$_[0]{count} = undef;
		return $c;
	}

	package fail_aggregate;

	sub new {
		my $class = shift;
		if ( ref $class ) {
			die "new() failed on request" if $class->{'fail'} eq 'new';
			return undef if $class->{'fail'} eq 'undef';
			return bless { %$class }, ref $class;
		} else {
			return bless { 'fail' => $_[0] }, $class;
		}
	}

	sub step {
		die "step() failed on request" if $_[0]{fail} eq 'step';
	}

	sub finalize {
		die "finalize() failed on request" if $_[0]{fail} eq 'finalize';
	}
}

foreach my $call_func (@CALL_FUNCS) {
	my $dbh = connect_ok( PrintError => 0 );

	$dbh->do( "CREATE TABLE aggr_test ( field )" );
	foreach my $val ( qw/NULL 1 'test'/ ) {
	    $dbh->do( "INSERT INTO aggr_test VALUES ( $val )" );
	}

	ok($dbh->$call_func( "newcount", 0, "count_aggr", "create_aggregate" ));
	my $result = $dbh->selectrow_arrayref( "SELECT newcount() FROM aggr_test" );
	ok( $result && $result->[0] == 3 );

	# Make sure that the init() function is called correctly
	$result = $dbh->selectall_arrayref( "SELECT newcount() FROM aggr_test GROUP BY field" );
	ok( @$result == 3 && $result->[0][0] == 1 && $result->[1][0] == 1 );


	# Test aggregate on empty table
	$dbh->do( "DROP TABLE aggr_empty_test;" );
	$dbh->do( "CREATE TABLE aggr_empty_test ( field )" );
	$result = $dbh->selectrow_arrayref( "SELECT newcount() FROM aggr_empty_test" );
	ok( $result && !$result->[0] );
	# Make sure that the init() function is called correctly
	$result = $dbh->selectrow_arrayref( "SELECT newcount() FROM aggr_empty_test" );
	ok( $result && !$result->[0] );

	ok($dbh->$call_func( "defined", 1, 'obj_aggregate', "create_aggregate" ));
	$result = $dbh->selectrow_arrayref( "SELECT defined(field) FROM aggr_test" );
	ok( $result && $result->[0] == 2 );
	$result = $dbh->selectrow_arrayref( "SELECT defined(field) FROM aggr_test" );
	ok( $result && $result->[0] == 2 );
	$result = $dbh->selectrow_arrayref( "SELECT defined(field) FROM aggr_empty_test" );
	ok( $result && !$result->[0] );
	$result = $dbh->selectrow_arrayref( "SELECT defined(field) FROM aggr_empty_test" );
	ok( $result && !$result->[0] );

	my $last_warn;
	local $SIG{__WARN__} = sub { $last_warn = join "", @_ };
	foreach my $fail ( qw/ new step finalize/ ) {
	    $last_warn = '';  
	    my $aggr = new fail_aggregate( $fail );
	    ok($dbh->$call_func( "fail_$fail", -1, $aggr, 'create_aggregate' ));
	    $result = $dbh->selectrow_arrayref( "SELECT fail_$fail() FROM aggr_test" );
	#   ok( !$result && $DBI::errstr =~ /$fail\(\) failed on request/ );
	    ok( !defined $result->[0] && $last_warn =~ /$fail\(\) failed on request/ );

	    # No need to check this one, since step() will never be called
	    # on an empty table
	    next if $fail eq 'step';
	    $result = $dbh->selectrow_arrayref( "SELECT fail_$fail() FROM aggr_empty_test" );
	#    ok( !$result && $DBI::errstr =~ /$fail\(\) failed on request/ );
	    ok( !defined $result->[0] && $last_warn =~ /$fail\(\) failed on request/ );
	}

	my $aggr = new fail_aggregate( 'undef' );
	$last_warn = '';
	ok($dbh->$call_func( "fail_undef", -1, $aggr, 'create_aggregate' ));
	$result = $dbh->selectrow_arrayref( "SELECT fail_undef() FROM aggr_test" );
	# ok( !$result && $DBI::errstr =~ /new\(\) should return a blessed reference/ );
	ok( !defined $result->[0] && $last_warn =~ /new\(\) should return a blessed reference/ );

	$dbh->disconnect;
}
