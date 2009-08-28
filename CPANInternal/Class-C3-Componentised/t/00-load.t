#!perl -T

use Test::More tests => 1;

BEGIN {
	use_ok( 'Class::C3::Componentised' );
}

diag( "Testing Class::C3::Componentised $Class::C3::Componentised::VERSION, Perl $], $^X" );
