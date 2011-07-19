#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
	$ENV{PERL_PARAMS_UTIL_PP} ||= 0;
}

use Test::More tests => 23;
use File::Spec::Functions ':ALL';
BEGIN {
	ok( ! defined &_HANDLE, '_HANDLE does not exist' );
	use_ok('Params::Util', qw(_HANDLE));
	ok( defined &_HANDLE, '_HANDLE imported ok' );
}

# Import refaddr to make certain we have it
use Scalar::Util 'refaddr';





#####################################################################
# Preparing

my $readfile  = catfile( 't', 'handles', 'readfile.txt'  );
ok( -f $readfile, "$readfile exists" );
my $writefile = catfile( 't', 'handles', 'writefile.txt' );
      if ( -f $writefile ) { unlink $writefile };
END { if ( -f $writefile ) { unlink $writefile }; }
ok( ! -e $writefile, "$writefile does not exist" );

sub is_handle {
	my $maybe   = shift;
	my $message = shift || 'Is a file handle';
	my $result  = _HANDLE($maybe);
	ok( defined $result, '_HANDLE does not return undef' );
	is( refaddr($result), refaddr($maybe), '_HANDLE returns the passed value' );
}

sub not_handle {
	my $maybe   = shift;
	my $message = shift || 'Is not a file handle';
	my $result  = _HANDLE($maybe);
	ok( ! defined $result, '_HANDLE returns undef' );
}





#####################################################################
# Basic Filesystem Handles

# A read filehandle
SCOPE: {
	local *HANDLE;
	open( HANDLE, $readfile );
	is_handle( \*HANDLE, 'Ordinary read filehandle' );
	close HANDLE;
}

# A write filehandle
SCOPE: {
	local *HANDLE;
	open( HANDLE, "> $readfile" );
	is_handle( \*HANDLE, 'Ordinary read filehandle' );
	print HANDLE "A write filehandle";
	close HANDLE;
	if ( -f $writefile ) { unlink $writefile };
}

# On 5.8+ the new style filehandle
SKIP: {
	skip( "Skipping 5.8-style 'my \$fh' handles", 2 ) if $] < 5.008;
	open( my $handle, $readfile );
	is_handle( $handle, '5.8-style read filehandle' );
}





#####################################################################
# Things that are not file handles

foreach (
	undef, '', ' ', 'foo', 1, 0, -1, 1.23,
	[], {}, \'', bless( {}, "foo" )
) {
	not_handle( $_ );
}

