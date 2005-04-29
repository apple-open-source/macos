#!/bin/perl -w
# Extracts the named files into 'extractTest' subdir
# usage:
#	perl extract.pl [-j] zipfile.zip filename [...]
# if -j option given, discards paths.
#
# $Revision: 1.1 $
#
use strict;

my $dirName = 'extractTest';

use vars qw( $opt_j );
use Archive::Zip qw(:ERROR_CODES);
use Getopt::Std;

$opt_j = 0;
getopts('j');

if (@ARGV < 2)
{
	die <<EOF
	usage: perl extract.pl [-j] zipfile.zip filename [...]
	if -j option given, discards paths.
EOF
}

my $zip = Archive::Zip->new();
my $zipName = shift(@ARGV);
my $status = $zip->read( $zipName );
die "Read of $zipName failed\n" if $status != AZ_OK;

foreach my $memberName (@ARGV)
{
	print "Extracting $memberName\n";
	$status = $opt_j 
		? $zip->extractMemberWithoutPaths($memberName)
		: $zip->extractMember($memberName);
	die "Extracting $memberName from $zipName failed\n" if $status != AZ_OK;
}
