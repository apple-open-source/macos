# Test Archive::Zip::Tree module
# $Revision: 1.1 $
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl t/testTree.t'
# vim: ts=4 sw=4 ft=perl

$^W = 1;
$| = 1;
use strict;
use Test;
use Archive::Zip qw( :ERROR_CODES :CONSTANTS );
use FileHandle;
use File::Spec 0.8;

my $zip;
my @memberNames;

sub makeZip
{
	my ($src, $dest, $pred) = @_;
	$zip = Archive::Zip->new();
	$zip->addTree($src, $dest, $pred);
	@memberNames = $zip->memberNames();
}

sub makeZipAndLookFor
{
	my ($src, $dest, $pred, $lookFor) = @_;
	makeZip($src, $dest, $pred);
	ok( @memberNames );
	ok( (grep { $_ eq $lookFor } @memberNames) == 1 )
		or print STDERR "Can't find $lookFor in (" . join(",", @memberNames) . ")\n";
}

BEGIN { plan tests => 6, todo => [] }

BEGIN { require 't/common.pl' }

use constant FILENAME => File::Spec->catfile(TESTDIR, 'testing.txt');

my ($testFileVolume, $testFileDirs, $testFileName) = File::Spec->splitpath($0);

makeZipAndLookFor('.', '', sub { print "file $_\n"; -f && /\.t$/ }, 't/test.t' );
makeZipAndLookFor('.', 'e/', sub { -f && /\.t$/ }, 'e/t/test.t');
makeZipAndLookFor('./t', '', sub { -f && /\.t$/ }, 'test.t' );
