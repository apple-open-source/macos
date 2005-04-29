#!/bin/perl -w
# Creates a zip file, adding the given directories and files.
# Usage:
#	perl zip.pl zipfile.zip file [...]

use strict;
use Archive::Zip qw(:ERROR_CODES :CONSTANTS);

die "usage: $0 zipfile.zip file [...]\n"
	if (scalar(@ARGV) < 2);

my $zipName = shift(@ARGV);
my $zip = Archive::Zip->new();

foreach my $memberName (map { glob } @ARGV)
{
	if (-d $memberName )
	{
		warn "Can't add tree $memberName\n"
			if $zip->addTree( $memberName, $memberName ) != AZ_OK;
	}
	else
	{
		$zip->addFile( $memberName )
			or warn "Can't add file $memberName\n";
	}
}

my $status = $zip->writeToFileNamed($zipName);
exit $status;
