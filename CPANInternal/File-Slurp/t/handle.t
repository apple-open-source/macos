#!/usr/local/bin/perl -w

use strict ;

use Carp ;
use POSIX qw( :fcntl_h ) ;
use Socket ;
use Symbol ;
use Test::More ;

# in case SEEK_SET isn't defined in older perls. it seems to always be 0

BEGIN {
	*SEEK_SET = sub { 0 } unless eval { defined SEEK_SET() } ;
}

my @pipe_data = (
	'',
	'abc',
	'abc' x 100_000,
	'abc' x 1_000_000,
) ;

#plan( tests => 2 + @pipe_data ) ;
plan( tests => 1 + scalar @pipe_data ) ;


use_ok( 'File::Slurp', )  ;

#test_data_slurp() ;

#test_fork_pipe_slurp() ;

SKIP: {

	eval { test_socketpair_slurp() } ;

	skip "socketpair not found in this Perl", scalar( @pipe_data ) if $@ ;
}

sub test_socketpair_slurp {

	foreach my $data ( @pipe_data ) {

		my $size = length( $data ) ;

		my $read_fh = gensym ;
		my $write_fh = gensym ;

		socketpair( $read_fh, $write_fh,
				AF_UNIX, SOCK_STREAM, PF_UNSPEC);
                
		if ( fork() ) {

#warn "PARENT SOCKET\n" ;
			close( $write_fh ) ;
			my $read_buf = read_file( $read_fh ) ;

			is( $read_buf, $data,
				"socket slurp/spew of $size bytes" ) ;

		}
		else {

#child
#warn "CHILD SOCKET\n" ;
			close( $read_fh ) ;
			write_file( $write_fh, $data ) ;
			exit() ;
		}
	}
}

sub test_data_slurp {

	my $data_seek = tell( \*DATA );

# first slurp in the lines 
	my @slurp_lines = read_file( \*DATA ) ;

# now seek back and read all the lines with the <> op and we make
# golden data sets

	seek( \*DATA, $data_seek, SEEK_SET ) || die "seek $!" ;
	my @data_lines = <DATA> ;
	my $data_text = join( '', @data_lines ) ;

# now slurp in as one string and test

	sysseek( \*DATA, $data_seek, SEEK_SET ) || die "seek $!" ;
	my $slurp_text = read_file( \*DATA ) ;
	is( $slurp_text, $data_text, 'scalar slurp DATA' ) ;

# test the array slurp

	ok( eq_array( \@data_lines, \@slurp_lines ), 'list slurp of DATA' ) ;
}

sub test_fork_pipe_slurp {

	foreach my $data ( @pipe_data ) {

		test_to_pipe( $data ) ;
		test_from_pipe( $data ) ;
	}
}


sub test_from_pipe {

	my( $data ) = @_ ;

	my $size = length( $data ) ;

	if ( pipe_from_fork( \*READ_FH ) ) {

# parent
		my $read_buf = read_file( \*READ_FH ) ;
warn "PARENT read\n" ;

		is( $read_buf, $data, "pipe slurp/spew of $size bytes" ) ;

		close \*READ_FH ;
#		return ;
	}
	else {
# child
warn "CHILD write\n" ;
	#	write_file( \*STDOUT, $data ) ;
		syswrite( \*STDOUT, $data, length( $data ) ) ;

		close \*STDOUT;
		exit(0);
	}
}


sub pipe_from_fork {

	my ( $parent_fh ) = @_ ;

	my $child = gensym ;

	pipe( $parent_fh, $child ) or die;

	my $pid = fork();
	die "fork() failed: $!" unless defined $pid;

	if ($pid) {

warn "PARENT\n" ;
		close $child;
		return $pid ;
	}

warn "CHILD FILENO ", fileno($child), "\n" ;
	close $parent_fh ;
	open(STDOUT, ">&=" . fileno($child)) or die "no fileno" ;

	return ;
}


sub test_to_pipe {

	my( $data ) = @_ ;

	my $size = length( $data ) ;

	if ( pipe_to_fork( \*WRITE_FH ) ) {

# parent
		syswrite( \*WRITE_FH, $data, length( $data ) ) ;
#		write_file( \*WRITE_FH, $data ) ;
warn "PARENT write\n" ;

#		is( $read_buf, $data, "pipe slurp/spew of $size bytes" ) ;

		close \*WRITE_FH ;
#		return ;
	}
	else {
# child
warn "CHILD read FILENO ", fileno(\*STDIN), "\n" ;

		my $read_buf = read_file( \*STDIN ) ;
		is( $read_buf, $data, "pipe slurp/spew of $size bytes" ) ;
		close \*STDIN;
		exit(0);
	}
}

sub pipe_to_fork {
	my ( $parent_fh ) = @_ ;

	my $child = gensym ;

	pipe( $child, $parent_fh ) or die ;

	my $pid = fork();
	die "fork() failed: $!" unless defined $pid;

	if ( $pid ) {
		close $child;
		return $pid ;
	}

	close $parent_fh ;
	open(STDIN, "<&=" . fileno($child)) or die;

	return ;
}

__DATA__
line one
second line
more lines
still more

enough lines

we don't test long handle slurps from DATA since i would have to type
too much stuff :-)

so we will stop here
