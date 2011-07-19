#!/usr/local/bin/perl -w

use strict ;

use Carp ;
use Test::More tests => 2 ;

my $proc_file = "/proc/$$/auxv" ;

BEGIN{ 
	use_ok( 'File::Slurp' ) ;
}

SKIP: {

	unless ( -r $proc_file ) {

		skip "can't find pseudo file $proc_file", 1 ;
	}

	test_pseudo_file() ;
}

sub test_pseudo_file {

	my $data_do = do{ local( @ARGV, $/ ) = $proc_file; <> } ;

	my $data_slurp = read_file( $proc_file ) ;

	is( $data_do, $data_slurp, 'pseudo' ) ;
}
