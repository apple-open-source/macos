#!/usr/local/bin/perl -w

use strict ;

use Test::More tests => 2 ;

BEGIN{ 
	use_ok( 'File::Slurp', qw( write_file slurp ) ) ;
}

my $data = <<TEXT ;
line 1
more text
TEXT

my $file = 'xxx' ;

write_file( $file, $data ) ;
my $read_buf = slurp( $file ) ;
is( $read_buf, $data, 'slurp alias' ) ;

unlink $file ;
