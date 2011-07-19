#!/usr/local/bin/perl

use strict ;

use Benchmark qw( timethese cmpthese ) ;
use Carp ;
use FileHandle ;
use Fcntl qw( :DEFAULT :seek );

use File::Slurp () ;

my $dur = shift || -2 ;

my $file = 'slurp_data' ;

my @lines = ( 'abc' x 30 . "\n")  x 100 ;
my $text = join( '', @lines ) ;

bench_list_spew( 'SHORT' ) ;
bench_scalar_spew( 'SHORT' ) ;

File::Slurp::write_file( $file, $text ) ;

bench_scalar_slurp( 'SHORT' ) ;
bench_list_slurp( 'SHORT' ) ;

@lines = ( 'abc' x 40 . "\n")  x 1000 ;
$text = join( '', @lines ) ;

bench_list_spew( 'LONG' ) ;
bench_scalar_spew( 'LONG' ) ;

File::Slurp::write_file( $file, $text ) ;

bench_scalar_slurp( 'LONG' ) ;
bench_list_slurp( 'LONG' ) ;

exit ;

sub bench_list_spew {

	my ( $size ) = @_ ;

	print "\n\nList Spew of $size file\n\n" ;

	my $result = timethese( $dur, {

 		new =>
 	    		sub { File::Slurp::write_file( $file, @lines ) },

		print_file =>
	    		sub { print_file( $file, @lines ) },

		print_join_file =>
	    		sub { print_join_file( $file, @lines ) },

		syswrite_file =>
	    		sub { syswrite_file( $file, @lines ) },

		cpan_write_file =>
			sub { cpan_write_file( $file, @lines ) },

	} ) ;

	cmpthese( $result ) ;
}

sub bench_scalar_spew {

	my ( $size ) = @_ ;

	print "\n\nScalar Spew of $size file\n\n" ;

	my $result = timethese( $dur, {

 		new =>
 	    		sub { File::Slurp::write_file( $file, $text ) },

 		new_ref =>
 	    		sub { File::Slurp::write_file( $file, \$text ) },

		print_file =>
	    		sub { print_file( $file, $text ) },

		print_join_file =>
	    		sub { print_join_file( $file, $text ) },

		syswrite_file =>
	    		sub { syswrite_file( $file, $text ) },

		syswrite_file2 =>
	    		sub { syswrite_file2( $file, $text ) },

		cpan_write_file =>
			sub { cpan_write_file( $file, $text ) },

	} ) ;

	cmpthese( $result ) ;
}

sub bench_scalar_slurp {

	my ( $size ) = @_ ;

	print "\n\nScalar Slurp of $size file\n\n" ;

	my $buffer ;

	my $result = timethese( $dur, {

		new =>
	    		sub { my $text = File::Slurp::read_file( $file ) },

		new_buf_ref =>
	    		sub { my $text ;
			   File::Slurp::read_file( $file, buf_ref => \$text ) },
		new_buf_ref2 =>
	    		sub { 
			   File::Slurp::read_file( $file, buf_ref => \$buffer ) },
		new_scalar_ref =>
	    		sub { my $text =
			    File::Slurp::read_file( $file, scalar_ref => 1 ) },

		read_file =>
	    		sub { my $text = read_file( $file ) },

		sysread_file =>
	    		sub { my $text = sysread_file( $file ) },

		cpan_read_file =>
			sub { my $text = cpan_read_file( $file ) },

		cpan_slurp =>
			sub { my $text = cpan_slurp_to_scalar( $file ) },

		file_contents =>
			sub { my $text = file_contents( $file ) },

		file_contents_no_OO =>
			sub { my $text = file_contents_no_OO( $file ) },
	} ) ;

	cmpthese( $result ) ;
}

sub bench_list_slurp {

	my ( $size ) = @_ ;

	print "\n\nList Slurp of $size file\n\n" ;

	my $result = timethese( $dur, {

		new =>
	    		sub { my @lines = File::Slurp::read_file( $file ) },

		new_array_ref =>
	    		sub { my $lines_ref =
			     File::Slurp::read_file( $file, array_ref => 1 ) },

		new_in_anon_array =>
	    		sub { my $lines_ref =
			     [ File::Slurp::read_file( $file ) ] },

		read_file =>
	    		sub { my @lines = read_file( $file ) },

		sysread_file =>
	    		sub { my @lines = sysread_file( $file ) },

		cpan_read_file =>
			sub { my @lines = cpan_read_file( $file ) },

		cpan_slurp_to_array =>
			sub { my @lines = cpan_slurp_to_array( $file ) },

		cpan_slurp_to_array_ref =>
			sub { my $lines_ref = cpan_slurp_to_array( $file ) },
	} ) ;

	cmpthese( $result ) ;
}

######################################
# uri's old fast slurp

sub read_file {

	my( $file_name ) = shift ;

	local( *FH ) ;
	open( FH, $file_name ) || carp "can't open $file_name $!" ;

	return <FH> if wantarray ;

	my $buf ;

	read( FH, $buf, -s FH ) ;
	return $buf ;
}

sub sysread_file {

	my( $file_name ) = shift ;

	local( *FH ) ;
	open( FH, $file_name ) || carp "can't open $file_name $!" ;

	return <FH> if wantarray ;

	my $buf ;

	sysread( FH, $buf, -s FH ) ;
	return $buf ;
}

######################################
# from File::Slurp.pm on cpan

sub cpan_read_file
{
	my ($file) = @_;

	local($/) = wantarray ? $/ : undef;
	local(*F);
	my $r;
	my (@r);

	open(F, "<$file") || croak "open $file: $!";
	@r = <F>;
	close(F) || croak "close $file: $!";

	return $r[0] unless wantarray;
	return @r;
}

sub cpan_write_file
{
	my ($f, @data) = @_;

	local(*F);

	open(F, ">$f") || croak "open >$f: $!";
	(print F @data) || croak "write $f: $!";
	close(F) || croak "close $f: $!";
	return 1;
}


######################################
# from Slurp.pm on cpan

sub slurp { 
    local( $/, @ARGV ) = ( wantarray ? $/ : undef, @_ ); 
    return <ARGV>;
}

sub cpan_slurp_to_array {
    my @array = slurp( @_ );
    return wantarray ? @array : \@array;
}

sub cpan_slurp_to_scalar {
    my $scalar = slurp( @_ );
    return $scalar;
}

######################################
# very slow slurp code used by a client

sub file_contents {
    my $file = shift;
    my $fh = new FileHandle $file or
        warn("Util::file_contents:Can't open file $file"), return '';
    return join '', <$fh>;
}

# same code but doesn't use FileHandle.pm

sub file_contents_no_OO {
    my $file = shift;

	local( *FH ) ;
	open( FH, $file ) || carp "can't open $file $!" ;

    return join '', <FH>;
}

##########################

sub print_file {

	my( $file_name ) = shift ;

	local( *FH ) ;

	open( FH, ">$file_name" ) || carp "can't create $file_name $!" ;

	print FH @_ ;
}

sub print_file2 {

	my( $file_name ) = shift ;

	local( *FH ) ;

	my $mode = ( -e $file_name ) ? '<' : '>' ;

	open( FH, "+$mode$file_name" ) || carp "can't create $file_name $!" ;

	print FH @_ ;
}

sub print_join_file {

	my( $file_name ) = shift ;

	local( *FH ) ;

	my $mode = ( -e $file_name ) ? '<' : '>' ;

	open( FH, "+$mode$file_name" ) || carp "can't create $file_name $!" ;

	print FH join( '', @_ ) ;
}


sub syswrite_file {

	my( $file_name ) = shift ;

	local( *FH ) ;

	open( FH, ">$file_name" ) || carp "can't create $file_name $!" ;

	syswrite( FH, join( '', @_ ) ) ;
}

sub syswrite_file2 {

	my( $file_name ) = shift ;

	local( *FH ) ;

	sysopen( FH, $file_name, O_WRONLY | O_CREAT ) ||
				carp "can't create $file_name $!" ;

	syswrite( FH, join( '', @_ ) ) ;
}
