#!/usr/local/bin/perl -w

use strict ;

use Carp ;
use Socket ;
use Symbol ;
use Test::More tests => 7 ;

BEGIN{ 
	use_ok( 'File::Slurp', ) ;
}

my $data = <<TEXT ;
line 1
more text
TEXT

foreach my $file ( qw( stdin STDIN stdout STDOUT stderr STDERR ) ) {

	write_file( $file, $data ) ;
	my $read_buf = read_file( $file ) ;
	is( $read_buf, $data, 'read/write of file [$file]' ) ;

	unlink $file ;
}
