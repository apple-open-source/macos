#!/usr/bin/perl -w
use strict;
use Archive::Zip qw(:CONSTANTS :ERROR_CODES);
use IO::Scalar;
use IO::File;

# test writing to a scalar
my $zipContents = '';
my $SH = IO::Scalar->new(\$zipContents);

my $zip = Archive::Zip->new();
my $member = $zip->addString('a' x 300, 'bunchOfAs.txt');
$member->desiredCompressionMethod(COMPRESSION_DEFLATED);
$member = $zip->addString('b' x 300, 'bunchOfBs.txt');
$member->desiredCompressionMethod(COMPRESSION_DEFLATED);
my $status = $zip->writeToFileHandle( $SH );

my $file = IO::File->new('test.zip', 'w');
binmode($file);
$file->print($zipContents);
$file->close();

