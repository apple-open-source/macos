#!perl -w

use strict;
use Test::More tests => 1;

BEGIN {
	use_ok( 'File::ExtAttr' );
}

diag( "Testing File::ExtAttr $File::ExtAttr::VERSION, Perl $], $^X" );
