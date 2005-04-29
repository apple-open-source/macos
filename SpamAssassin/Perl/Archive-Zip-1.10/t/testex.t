# Test examples
# $Revision: 1.1 $
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl t/testex.t'
# vim: ts=4 sw=4 ft=perl

$^W = 1;
$|  = 1;
use strict;
use Test;
use Archive::Zip qw( :ERROR_CODES :CONSTANTS );
use File::Spec;
use IO::File;

BEGIN { plan tests => 15, todo => [] }

BEGIN { require 't/common.pl' }

sub runPerlCommand
{
	my $libs = join ( ' -I', @INC );
	my $cmd    = "\"$^X\" \"-I$libs\" -w \"". join('" "', @_). '"';
	my $output = `$cmd`;
	return wantarray ? ( $?, $output ) : $?;
}

use constant FILENAME => File::Spec->catpath( '', TESTDIR, 'testing.txt' );
use constant ZFILENAME => TESTDIR . "/testing.txt"; # name in zip

my $zip = Archive::Zip->new();
$zip->addString( TESTSTRING, FILENAME );
$zip->writeToFileNamed(INPUTZIP);

my ( $status, $output );
my $fh = IO::File->new( "test.log", "w" );

ok( runPerlCommand( 'examples/copy.pl', INPUTZIP, OUTPUTZIP ), 0 );

ok( runPerlCommand( 'examples/extract.pl', OUTPUTZIP, ZFILENAME ), 0 );

ok( runPerlCommand( 'examples/mfh.pl', INPUTZIP ), 0 );

ok( runPerlCommand( 'examples/zip.pl', OUTPUTZIP, INPUTZIP, FILENAME ), 0 );

( $status, $output ) = runPerlCommand( 'examples/zipinfo.pl', INPUTZIP );
ok( $status, 0 );
$fh->print("zipinfo output:\n");
$fh->print($output);

( $status, $output ) = runPerlCommand( 'examples/ziptest.pl', INPUTZIP );
ok( $status, 0 );
$fh->print("ziptest output:\n");
$fh->print($output);

( $status, $output ) = runPerlCommand( 'examples/zipGrep.pl', '100', INPUTZIP );
ok( $status, 0 );
ok( $output, ZFILENAME . ":100\n" );

# calcSizes.pl
# creates test.zip, may be sensitive to /dev/null

# removed because requires IO::Scalar
# ok( runPerlCommand('examples/readScalar.pl'), 0 );

unlink(OUTPUTZIP);
ok( runPerlCommand( 'examples/selfex.pl', OUTPUTZIP, FILENAME ), 0 );
unlink(FILENAME);
ok( runPerlCommand(OUTPUTZIP), 0 );
my $fn =
  File::Spec->catpath( '', File::Spec->catdir( 'extracted', TESTDIR ),
	'testing.txt' );
ok( -f $fn, 1, "$fn exists" );

# unzipAll.pl
# updateZip.pl
# writeScalar.pl
# zipcheck.pl
# ziprecent.pl

unlink(OUTPUTZIP);
ok( runPerlCommand( 'examples/updateTree.pl', OUTPUTZIP, TESTDIR ), 0, "updateTree.pl create" );
ok( -f OUTPUTZIP, 1, "zip created" );
ok( runPerlCommand( 'examples/updateTree.pl', OUTPUTZIP, TESTDIR ), 0, "updateTree.pl update" );
ok( -f OUTPUTZIP, 1, "zip updated" );
unlink(OUTPUTZIP);
