#!/bin/perl -w
# Extracts all files from the given zip
# $Revision: 1.1 $
# usage:
#	perl unzipAll.pl [-j] zipfile.zip
# if -j option given, discards paths.
#
use strict;

use vars qw( $opt_j );
use Archive::Zip qw(:ERROR_CODES);
use Getopt::Std;

$opt_j = 0;
getopts('j');

if (@ARGV < 1)
{
	die <<EOF
	usage: perl $0 [-j] zipfile.zip
	if -j option given, discards paths.
EOF
}

my $zip = Archive::Zip->new();
my $zipName = shift(@ARGV);
my $status = $zip->read( $zipName );
die "Read of $zipName failed\n" if $status != AZ_OK;

$zip->extractTree();
