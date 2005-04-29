# Example of how to compute compressed sizes
# $Revision: 1.1 $
use strict;
use Archive::Zip qw(:ERROR_CODES);
use File::Spec;
my $zip = Archive::Zip->new();
my $blackHoleDevice = File::Spec->devnull();

$zip->addFile($_) foreach (<*.pl>);

# Write and throw the data away.
# after members are written, the writeOffset will be set
# to the compressed size.
$zip->writeToFileNamed($blackHoleDevice);

my $totalSize = 0;
my $totalCompressedSize = 0;
foreach my $member ($zip->members())
{
	$totalSize += $member->uncompressedSize;
	$totalCompressedSize += $member->_writeOffset;
	print "Member ", $member->externalFileName,
	" size=", $member->uncompressedSize,
	", writeOffset=", $member->_writeOffset,
	", compressed=", $member->compressedSize,
	"\n";
}

print "Total Size=", $totalSize, ", total compressed=", $totalCompressedSize, "\n";

$zip->writeToFileNamed('test.zip');
