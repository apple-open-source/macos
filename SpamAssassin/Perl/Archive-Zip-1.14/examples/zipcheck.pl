#!/bin/perl -w
# usage: valid zipname.zip
# exits with non-zero status if invalid zip
# status = 1: invalid arguments
# status = 2: generic error somewhere
# status = 3: format error
# status = 4: IO error
use strict;
use Archive::Zip qw(:ERROR_CODES);
use IO::Handle;
use File::Spec;

# instead of stack dump:
Archive::Zip::setErrorHandler( sub { warn shift() } );

my $nullFileName = File::Spec->devnull();
my $zip = Archive::Zip->new();
my $zipName = shift(@ARGV) || exit 1;
eval
{
	my $status = $zip->read( $zipName );
	exit $status if $status != AZ_OK;
};
if ($@) { warn 'error reading zip:', $@, "\n"; exit 2 }

eval
{
	foreach my $member ($zip->members)
	{
		my $fh = IO::File->new();
		$fh->open(">$nullFileName") || die "can't open $nullFileName\: $!\n";
		my $status = $member->extractToFileHandle($fh);
		if ($status != AZ_OK)
		{
			warn "Extracting ", $member->fileName(), " from $zipName failed\n";
			exit $status;
		}
	}
}
