# Shared defs for test programs

# Paths. Must make case-insensitive.
use constant TESTDIR   => 'testdir';
use constant INPUTZIP  => 'testin.zip';
use constant OUTPUTZIP => 'testout.zip';

use constant ZIP     => 'zip ';
use constant ZIPTEST => 'unzip -t ';

# 300-character test string
use constant TESTSTRING => join ( "\n", 1 .. 102 ) . "\n";
use constant TESTSTRINGLENGTH => length(TESTSTRING);

# CRC-32 should be ac373f32
use constant TESTSTRINGCRC => Archive::Zip::computeCRC32(TESTSTRING);

# This is so that it will work on other systems.
use constant CAT     => $^X . ' -pe "BEGIN{binmode(STDIN);binmode(STDOUT)}"';
use constant CATPIPE => '| ' . CAT . ' >';

use vars qw($zipWorks $testZipDoesntWork $catWorks);
local ( $zipWorks, $testZipDoesntWork, $catWorks );

# Run ZIPTEST to test a zip file.
sub testZip
{
	my $zipName = shift || OUTPUTZIP;
	if ($testZipDoesntWork)
	{
		return wantarray ? ( 0, '' ) : 0;
	}
	my $cmd = ZIPTEST . $zipName . ( $^O eq 'MSWin32' ? '' : ' 2>&1' );
	my $zipout = `$cmd`;
	return wantarray ? ( $?, $zipout ) : $?;
}

# Return the crc-32 of the given file (0 if empty or error)
sub fileCRC
{
	my $fileName = shift;
	local $/ = undef;
	my $fh = IO::File->new( $fileName, "r" );
	binmode($fh);
	return 0 if not defined($fh);
	my $contents = <$fh>;
	return Archive::Zip::computeCRC32($contents);
}

#--------- check to see if cat works

sub testCat
{
	my $fh = IO::File->new( CATPIPE . OUTPUTZIP );
	binmode($fh);
	my $testString = pack( 'C256', 0 .. 255 );
	my $testCrc = Archive::Zip::computeCRC32($testString);
	$fh->write( $testString, length($testString) ) or return 0;
	$fh->close();
	( -f OUTPUTZIP ) or return 0;
	my @stat = stat(OUTPUTZIP);
	$stat[7] == length($testString) or return 0;
	fileCRC(OUTPUTZIP) == $testCrc or return 0;
	unlink(OUTPUTZIP);
	return 1;
}

BEGIN
{
	$catWorks = testCat();
	warn( 'warning: ', CAT, " doesn't seem to work, may skip some tests" )
	  if !$catWorks;
}

#--------- check to see if zip works (and make INPUTZIP)

BEGIN
{
	unlink(INPUTZIP);
	my $cmd = ZIP . INPUTZIP . ' *' . ( $^O eq 'MSWin32' ? '' : ' 2>&1' );
	my $zipout = `$cmd`;
	$zipWorks = not $?;
	warn( 'warning: ', ZIP, " doesn't seem to work, may skip some tests" )
	  if not $zipWorks;
}

#--------- check to see if unzip -t works

BEGIN
{
	$testZipDoesntWork = 0;
	my ( $status, $zipout ) = testZip(INPUTZIP);
	$testZipDoesntWork = $status;
	warn( 'warning: ', ZIPTEST, " doesn't seem to work, may skip some tests" )
	  if $testZipDoesntWork;
}

1;
