#/usr/bin/perl -w
#
# Shows one way to write a self-extracting archive file.
# This is not intended for production use, and it always extracts to a
# subdirectory with a fixed name.
# Plus, it requires Perl and A::Z to be installed first.
#
# In general, you want to provide a stub that is platform-specific.
# You can use 'unzipsfx' that it provided with the Info-Zip unzip program.
# Get this from http://www.info-zip.org .
#
# $Revision: 1.1 $
#
use strict;

use Archive::Zip;
use IO::File;

# Make a self-extracting Zip file.

die "usage: $0 sfxname file [...]\n" unless @ARGV > 1;

my $outputName = shift();

my $zip = Archive::Zip->new();

foreach my $file (@ARGV)
{
	$zip->addFileOrDirectory($file);
}

my $fh = IO::File->new( $outputName, O_CREAT | O_WRONLY | O_TRUNC, 0777 )
  or die "Can't open $outputName\: $!\n";
binmode($fh);

# add self-extracting Perl code

while (<DATA>)
{
	$fh->print($_)
}

$zip->writeToFileHandle($fh);

$fh->close();

# below the __DATA__ line is the extraction stub:
__DATA__
#!/usr/local/bin/perl
# Self-extracting Zip file extraction stub
# Copyright (C) 2002 Ned Konz

use Archive::Zip qw(:ERROR_CODES);
use IO::File;
use File::Spec;

my $dir = 'extracted';
my $zip = Archive::Zip->new();
my $fh = IO::File->new($0) or die "Can't open $0\: $!\n";
die "Zip read error\n" unless $zip->readFromFileHandle($fh) == AZ_OK;

(mkdir($dir, 0777) or die "Can't create directory $dir\: $!\n") unless -d $dir;

for my $member ( $zip->members )
{
	$member->extractToFileNamed( File::Spec->catfile($dir,$member->fileName) );
}
__DATA__
