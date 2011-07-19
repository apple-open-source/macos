#!/usr/local/bin/perl -w

use strict ;

use Test::More ;
use Carp ;

my $file = 'slurp.data' ;
unlink $file ;

my @text_data = (
	[],
	[ 'a' x 8 ],
	[ "\n" x 5 ],
	[ map( "aaaaaaaa\n\n", 1 .. 3 ) ],
	[ map( "aaaaaaaa\n\n", 1 .. 3 ), 'aaaaaaaa' ],
 	[ map( "aaaaaaaa" . ( "\n"  x (2 + rand 3) ), 1 .. 100 ) ],
 	[ map( "aaaaaaaa" . ( "\n"  x (2 + rand 3) ), 1 .. 100 ), 'aaaaaaaa' ],
	[],
) ;

plan( tests => 1 + ( 3 * @text_data ) ) ;

use_ok( 'File::Slurp', ) ;


#print "# text slurp\n" ;

foreach my $data ( @text_data ) {

	test_text_slurp( $data ) ;
}


unlink $file ;

exit ;

sub test_text_slurp {

	my( $data_ref ) = @_ ;

	my @data_lines = @{$data_ref} ;
	my $data_text = join( '', @data_lines ) ;

	local( $/ ) = '' ;

	my $err = write_file( $file, $data_text ) ;
	ok( $err, 'write_file - ' . length $data_text ) ;


	my @array = read_file( $file ) ;
	ok( eq_array( \@array, \@data_lines ),
			'array read_file - ' . length $data_text ) ;

	print "READ:\n", map( "[$_]\n", @array ),
		 "EXP:\n", map( "[$_]\n", @data_lines )
			unless eq_array( \@array, \@data_lines ) ;

 	my $array_ref = read_file( $file, array_ref => 1 ) ;
	ok( eq_array( $array_ref, \@data_lines ),
 			'array ref read_file - ' . length $data_text ) ;

	return ;
}
