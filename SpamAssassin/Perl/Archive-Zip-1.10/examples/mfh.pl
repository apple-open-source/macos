# Prints messages on every chunk write.
# Usage:
# perl mfh.pl zipfile.zip
# $Revision: 1.1 $
use strict;
use Archive::Zip qw(:ERROR_CODES);
use Archive::Zip::MockFileHandle;

package NedsFileHandle;
use vars qw(@ISA);
@ISA = qw( Archive::Zip::MockFileHandle );

sub writeHook
{
	my $self = shift;
	my $bytes = shift;
	my $length = length($bytes);
	printf "write %d bytes (position now %d)\n", $length, $self->tell();
	return $length;
}

package main;

my $zip = Archive::Zip->new();
my $status = $zip->read($ARGV[0]);
exit $status if $status != AZ_OK;

my $fh = NedsFileHandle->new();
$zip->writeToFileHandle($fh, 0);
