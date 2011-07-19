#!perl -T

use Test::More tests => 1;

BEGIN {
	use_ok( 'Class::Unload' );
}

diag( "Testing Class::Unload $Class::Unload::VERSION, Perl $], $^X" );
