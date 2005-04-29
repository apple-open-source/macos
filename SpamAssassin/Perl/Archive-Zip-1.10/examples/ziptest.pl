#!/bin/perl -w
# $Revision: 1.1 $
# Lists the zipfile given as a first argument and tests CRC's.
# Usage:
#	perl ziptest.pl zipfile.zip

use strict;

use Archive::Zip qw(:ERROR_CODES :CONSTANTS);

package CRCComputingFileHandle;
use Archive::Zip::MockFileHandle;

use vars qw( @ISA );
@ISA = qw( Archive::Zip::MockFileHandle );

my $crc;

sub writeHook
{
	my $self = shift;
	my $bytes = shift;
	my $length = length($bytes);
	$crc = Archive::Zip::computeCRC32($bytes, $crc);
}

sub resetCRC { $crc = 0 }

sub crc { $crc }

package main;

die "usage: $0 zipfile.zip\n"
	if (scalar(@ARGV) != 1);

my $zip = Archive::Zip->new();
my $status = $zip->read( $ARGV[0] );
exit $status if $status != AZ_OK;

print " Length    Size         Last Modified         CRC-32  Name\n";
print "-------- --------  ------------------------  -------- ----\n";

my $fh = CRCComputingFileHandle->new();
my @errors;

foreach my $member ($zip->members())
{
	my $compressedSize = $member->compressedSize();
	$fh->resetCRC();
	$member->desiredCompressionMethod(COMPRESSION_STORED);
	$status = $member->extractToFileHandle($fh);
	exit $status if $status != AZ_OK;
	my $crc = $fh->crc();

	my $ct = scalar(localtime($member->lastModTime()));
	chomp($ct);

	printf("%8d %8d  %s  %08x %s\n",
		$member->uncompressedSize(),
		$compressedSize,
		$ct,
		$member->crc32(),
		$member->fileName()
		);

	if ($member->crc32() != $crc)
	{
		push(@errors,
			sprintf("Member %s CRC error: file says %08x computed: %08x\n",
				$member->fileName(), $member->crc32(), $crc));
	}
}

if (scalar(@errors))
{
	print join("\n", @errors);
	die "CRC errors found\n";
}
else
{
	print "All CRCs check OK\n";
}
