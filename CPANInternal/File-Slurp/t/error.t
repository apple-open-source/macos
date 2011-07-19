##!/usr/local/bin/perl -w

use strict ;

use Test::More tests => 10 ;
use Carp ;

BEGIN{ 
	use_ok( 'File::Slurp', ) ;
}
use File::Slurp ;


my $file = 'missing/file' ;
unlink $file ;

my %modes = (
	'croak' => \&test_croak,
	'carp' => \&test_carp,
	'quiet' => \&test_quiet,
) ;

while( my( $mode, $sub ) = each %modes ) {

	$sub->( 'read_file', \&read_file, $file, err_mode => $mode ) ;
	$sub->( 'write_file', \&write_file, $file,
					{ err_mode => $mode }, 'junk' ) ;
	$sub->( 'read_dir', \&read_dir, $file, err_mode => $mode ) ;
}


sub test_croak {

	my ( $name, $sub, @args ) = @_ ;

	eval {
		$sub->( @args ) ;
	} ;

	ok( $@, "$name can croak" ) ;
}

sub test_carp {

	my ( $name, $sub, @args ) = @_ ;

	local $SIG{__WARN__} = sub { ok( 1, "$name can carp" ) } ;

	$sub->( @args ) ;
}

sub test_quiet {

	my ( $name, $sub, @args ) = @_ ;

	local $SIG{__WARN__} = sub { ok( 0, "$name can be quiet" ) } ;

	eval {
		$sub->( @args ) ;
	} ;

	ok( !$@, "$name can be quiet" ) ;
}
