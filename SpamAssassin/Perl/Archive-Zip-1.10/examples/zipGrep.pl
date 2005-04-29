#!/usr/bin/perl -w
# This program searches for the given Perl regular expression in a Zip archive.
# Archive is assumed to contain text files.
# By Ned Konz, perl@bike-nomad.com
# Usage:
# 	perl zipGrep.pl 'pattern' myZip.zip
#
use strict;
use Archive::Zip qw(:CONSTANTS :ERROR_CODES);

if ( @ARGV != 2 )
{
	print <<EOF;
This program searches for the given Perl regular expression in a Zip archive.
Archive is assumed to contain text files.
Usage:
	perl $0 'pattern' myZip.zip
EOF
	exit 1;
}

my $pattern = shift;
$pattern = qr{$pattern};    # compile the regular expression
my $zipName = shift;

my $zip = Archive::Zip->new();
if ( $zip->read($zipName) != AZ_OK )
{
	die "Read error reading $zipName\n";
}

foreach my $member ( $zip->members() )
{
	my ( $bufferRef, $status, $lastChunk );
	my $memberName = $member->fileName();
	my $lineNumber = 1;
	$lastChunk = '';
	$member->desiredCompressionMethod(COMPRESSION_STORED);
	$status = $member->rewindData();
	die "rewind error $status" if $status != AZ_OK;

	while ( !$member->readIsDone() )
	{
		( $bufferRef, $status ) = $member->readChunk();
		die "readChunk error $status"
		  if $status != AZ_OK && $status != AZ_STREAM_END;

		my $buffer = $lastChunk . $$bufferRef;
		while ( $buffer =~ m{(.*$pattern.*\n)}mg )
		{
			print "$memberName:$1";
		}
		($lastChunk) = $$bufferRef =~ m{([^\n\r]+)\z};
	}

	$member->endRead();
}
