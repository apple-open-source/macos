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

eval { require B } ;

SKIP: {

	skip <<TEXT, 1 if $@ ;
B.pm not found in this Perl. Note this will cause slurping of
the DATA handle to fail.
TEXT

	test_data_scalar_slurp() ;
}

exit ;



exit ;

sub test_data_scalar_slurp {

	my $data_seek = tell( \*DATA );

# first slurp in the text
 
	my $slurp_text = read_file( \*DATA ) ;

# now we need to get the golden data

	seek( \*DATA, $data_seek, SEEK_SET ) || die "seek $!" ;
	my $data_text = join( '', <DATA> ) ;

	is( $slurp_text, $data_text, 'scalar slurp of DATA' ) ;
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
