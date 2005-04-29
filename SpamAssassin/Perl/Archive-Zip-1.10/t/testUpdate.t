# Test Archive::Zip updating
# $Revision: 1.1 $
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl t/testUpdate.t'
# vim: set ts=4 sw=4 ft=perl

$^W = 1;
$| = 1;
use strict;
use Test;
use Archive::Zip qw( :ERROR_CODES :CONSTANTS );
use IO::File;
use File::Spec 0.8;
use File::Find ();

BEGIN { plan tests => 12, todo => [] }

my ($testFileVolume, $testFileDirs, $testFileName) = File::Spec->splitpath($0);

my $zip = Archive::Zip->new();
my $testDir = File::Spec->catpath( $testFileVolume, $testFileDirs, '' );

my $numberOfMembers = 0;
my @memberNames;
sub countMembers { unless ($_ eq '.')
	{ push(@memberNames, $_); $numberOfMembers++; } };
File::Find::find( \&countMembers, $testDir );
ok( $numberOfMembers > 1, 1, 'not enough members to test');

# an initial updateTree() should act like an addTree()
ok( $zip->updateTree( $testDir ), AZ_OK, 'initial updateTree failed' );
ok( scalar($zip->members()), $numberOfMembers, 'wrong number of members after create' );

my $firstFile = $memberNames[0];
my $firstMember = ($zip->members())[0];

ok( $firstFile, $firstMember->fileName(), 'member name wrong');

# add a file to the directory
$testFileName = File::Spec->catpath( $testFileVolume, $testFileDirs, 'xxxxxx' );
my $fh = IO::File->new( $testFileName, 'w');
$fh->print('xxxx');
undef($fh);
ok( -f $testFileName, 1, "creating $testFileName failed");

# Then update it. It should be added.
ok( $zip->updateTree( $testDir ), AZ_OK, 'updateTree failed' );
ok( scalar($zip->members()), $numberOfMembers + 1, 'wrong number of members after update' );

# Delete the file.
unlink($testFileName);
ok( -f $testFileName, undef, "deleting $testFileName failed");

# updating without the mirror option should keep the members
ok( $zip->updateTree( $testDir ), AZ_OK, 'updateTree failed' );
ok( scalar($zip->members()), $numberOfMembers + 1, 'wrong number of members after update' );

# now try again with the mirror option; should delete the last file.
ok( $zip->updateTree( $testDir, undef, undef, 1 ), AZ_OK, 'updateTree failed' );
ok( scalar($zip->members()), $numberOfMembers, 'wrong number of members after mirror' );

