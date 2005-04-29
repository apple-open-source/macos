#! /usr/bin/perl -w
# Print out information about a ZIP file.
# Note that this buffers the entire file into memory!
# usage:
# perl examples/zipinfo.pl zipfile.zip

use strict;

use Data::Dumper ();
use FileHandle;
use Archive::Zip qw(:ERROR_CODES :CONSTANTS :PKZIP_CONSTANTS);
use Archive::Zip::BufferedFileHandle;

$| = 1;

# use constant END_OF_CENTRAL_DIRECTORY_SIGNATURE_STRING;
use constant CENTRAL_DIRECTORY_FILE_HEADER_SIGNATURE_STRING => pack( SIGNATURE_FORMAT,
	CENTRAL_DIRECTORY_FILE_HEADER_SIGNATURE );
use constant LOCAL_FILE_HEADER_SIGNATURE_STRING => pack( SIGNATURE_FORMAT,
	LOCAL_FILE_HEADER_SIGNATURE );

$Data::Dumper::Useqq = 1;	# enable double-quotes for string values
$Data::Dumper::Indent = 1;

my $zip = Archive::Zip->new();
my $zipFileName = shift(@ARGV);

my $fh = Archive::Zip::BufferedFileHandle->new();
$fh->readFromFile($zipFileName) or exit($!);

my $status = $zip->_findEndOfCentralDirectory($fh);
die("can't find EOCD\n") if $status != AZ_OK;

my $eocdPosition = $fh->tell( );

$status = $zip->_readEndOfCentralDirectory($fh);
die("can't read EOCD\n") if $status != AZ_OK;

my $zipDumper = Data::Dumper->new([$zip], ['ZIP']);
$zipDumper->Seen({ ref($fh), $fh });
print $zipDumper->Dump(), "\n";

my $expectedEOCDPosition = $zip->centralDirectoryOffsetWRTStartingDiskNumber()
	+ $zip->centralDirectorySize();

my $eocdOffset = $zip->{eocdOffset} = $eocdPosition - $expectedEOCDPosition;

if ($eocdOffset)
{
	printf "Expected EOCD at %d (0x%x) but found it at %d (0x%x)\n",
		($expectedEOCDPosition) x 2, ($eocdPosition) x 2;
}
else
{
	printf("Found EOCD at %d (0x%x)\n\n", ($eocdPosition) x 2);
}

my $contents = $fh->contents();
my $offset = $eocdPosition + $eocdOffset - 1;
my $cdPos;
my @members;
my $numberOfMembers = $zip->numberOfCentralDirectoriesOnThisDisk(); 
foreach my $n (0 .. $numberOfMembers - 1)
{
	my $index = $numberOfMembers - $n;
	$cdPos = rindex($contents,
		CENTRAL_DIRECTORY_FILE_HEADER_SIGNATURE_STRING, $offset);
	if ($cdPos < 0)
	{
		print "No central directory found for member #$index\n";
		last;
	}
	else
	{
		print "Found central directory for member #$index at $cdPos\n";
		$fh->seek($cdPos + SIGNATURE_LENGTH, 0);	# SEEK_SET
		my $newMember = $zip->ZIPMEMBERCLASS->_newFromZipFile(
			$fh, "($zipFileName)" );
		$status = $newMember->_readCentralDirectoryFileHeader();
		if ($status != AZ_OK and $status != AZ_STREAM_END)
		{
			printf "read CD header status=%d\n", $status;
			last;
		}
		unshift(@members, $newMember);

		my $memberDumper = Data::Dumper->new([$newMember], ['CDMEMBER' . $index ]);
		$memberDumper->Seen({ ref($fh), $fh });
		print $memberDumper->Dump(), "\n";
	}
	$offset = $cdPos - 1;
}

if ($cdPos >= 0 and 
	$cdPos != $zip->centralDirectoryOffsetWRTStartingDiskNumber())
{
	printf "Expected to find central directory at %d (0x%x), but found it at %d (0x%x)\n",
		($zip->centralDirectoryOffsetWRTStartingDiskNumber()) x 2,
		($cdPos) x 2;
}

print "\n";

# Now read the local headers

foreach my $n (0 .. $#members)
{
	my $member = $members[$n];
	$fh->seek($member->localHeaderRelativeOffset() + $eocdOffset + SIGNATURE_LENGTH, 0);
	$status = $member->_readLocalFileHeader();
	if ($status != AZ_OK and $status != AZ_STREAM_END)
	{
		printf "member %d read header status=%d\n", $n+1, $status;
		last;
	}

	my $memberDumper = Data::Dumper->new([$member], ['LHMEMBER' . ($n + 1)]);
	$memberDumper->Seen({ ref($fh), $fh });
	print $memberDumper->Dump(), "\n";

	my $endOfMember = $member->localHeaderRelativeOffset()
		+ $member->_localHeaderSize()
		+ $member->compressedSize();

	if ($endOfMember > $cdPos
		or ($n < $#members and 
			$endOfMember > $members[$n+1]->localHeaderRelativeOffset()))
	{
		print "Error: ";
	}
	printf("End of member: %d, CD at %d", $endOfMember, $cdPos);
	if ( $n < $#members )
	{
		printf(", next member starts at %d",
			$members[$n+1]->localHeaderRelativeOffset());
	}
	print("\n\n");
}

# vim: ts=4 sw=4
