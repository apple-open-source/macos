#!/usr/local/bin/perl -w

use strict ;

use Carp ;
use POSIX qw( :fcntl_h ) ;
use Test::More tests => 2 ;

# in case SEEK_SET isn't defined in older perls. it seems to always be 0

BEGIN {

	*SEEK_SET = sub { 0 } unless eval { defined SEEK_SET() } ;
}

BEGIN{ 
	use_ok( 'File::Slurp', ) ;
}

SKIP: {

	eval { require B } ;

	skip <<TEXT, 1 if $@ ;
B.pm not found in this Perl. This will cause slurping of
the DATA handle to fail.
TEXT

	test_data_list_slurp() ;
}

exit ;


sub test_data_list_slurp {

	my $data_seek = tell( \*DATA );

# first slurp in the lines
 
	my @slurp_lines = read_file( \*DATA ) ;

# now seek back and read all the lines with the <> op and we make
# golden data sets

	seek( \*DATA, $data_seek, SEEK_SET ) || die "seek $!" ;
	my @data_lines = <DATA> ;

# test the array slurp

	ok( eq_array( \@data_lines, \@slurp_lines ), 'list slurp of DATA' ) ;
}

__DATA__
line one
second line
more lines
still more

enough lines

we can't test long handle slurps from DATA since i would have to type
too much stuff

so we will stop here
