#!perl -T

use Test::More tests => 1;

BEGIN {
	use_ok( 'Mail::Sender' );
}

diag( "Testing Mail::Sender $Mail::Sender::VERSION, Perl $], $^X" );
