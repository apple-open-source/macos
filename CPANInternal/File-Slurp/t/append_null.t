#!/usr/local/bin/perl -w

use strict ;

use Test::More tests => 2 ;

BEGIN{ 
	use_ok( 'File::Slurp', qw( read_file write_file append_file ) ) ;
}

my $data = <<TEXT ;
line 1
more text
TEXT

my $file = 'xxx' ;

unlink $file ;

my $err = write_file( $file, $data ) ;
append_file( $file, '' ) ;

my $read_data = read_file( $file ) ;

is( $data, $read_data ) ;


unlink $file ;
