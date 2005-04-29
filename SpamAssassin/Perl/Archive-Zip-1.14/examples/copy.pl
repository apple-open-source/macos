# Copies a zip file to another.
# Usage:
# perl copy.pl input.zip output.zip
# $Revision: 1.1 $

use Archive::Zip qw(:ERROR_CODES);

die "usage: perl copy.pl input.zip output.zip\n"
	if scalar(@ARGV) != 2;

my $zip = Archive::Zip->new();

my $status = $zip->read($ARGV[0]);
die("read $ARGV[0] failed: $status\n") if $status != AZ_OK;

$status = $zip->writeToFileNamed($ARGV[1]);
die("writeToFileNamed $ARGV[1] failed: $status\n") if $status != AZ_OK;
