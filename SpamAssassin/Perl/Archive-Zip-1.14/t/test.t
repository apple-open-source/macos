# $Revision: 1.1 $
# Before `make install' is performed this script should be runnable
# with `make test'. After `make install' it should work as
# `perl t/test.t'
# vim: ts=4 sw=4 ft=perl

$^W = 1;
use strict;
use Test;
use Archive::Zip qw( :ERROR_CODES :CONSTANTS );
use FileHandle;
use File::Path;
use File::Spec;

BEGIN { plan tests => 123, todo => [] }

BEGIN { require 't/common.pl' or die "Can't get t/common.pl" }

my ($zip, @members, $numberOfMembers, $status, $member, $zipout,
	$memberName, @memberNames);

#--------- check CRC
ok(TESTSTRINGCRC, 0xac373f32);

#--------- empty file
# new	# Archive::Zip
# new	# Archive::Zip::Archive
$zip = Archive::Zip->new();
ok(defined($zip));

# members	# Archive::Zip::Archive
@members = $zip->members();
ok(scalar(@members), 0);

# numberOfMembers	# Archive::Zip::Archive
$numberOfMembers = $zip->numberOfMembers();
ok($numberOfMembers, 0);

# writeToFileNamed	# Archive::Zip::Archive
$status = $zip->writeToFileNamed( OUTPUTZIP );
ok($status, AZ_OK);

($status, $zipout) = testZip();
# STDERR->print("status= $status, out=$zipout\n");
skip($testZipDoesntWork, $status != 0);
# unzip -t returns error code=1 for warning on empty

#--------- add a directory
$memberName = TESTDIR . '/';
my $dirName = TESTDIR;

# addDirectory	# Archive::Zip::Archive
# new	# Archive::Zip::Member
$member = $zip->addDirectory($memberName);
ok(defined($member));

ok($member->fileName(), $memberName);

# members	# Archive::Zip::Archive
@members = $zip->members();
ok(scalar(@members), 1);
ok($members[0], $member);

# numberOfMembers	# Archive::Zip::Archive
$numberOfMembers = $zip->numberOfMembers();
ok($numberOfMembers, 1);

# writeToFileNamed	# Archive::Zip::Archive
$status = $zip->writeToFileNamed( OUTPUTZIP );
ok($status, AZ_OK);

($status, $zipout) = testZip();
# STDERR->print("status= $status, out=$zipout\n");
skip($testZipDoesntWork, $status, 0);

#--------- extract the directory by name
rmtree([ TESTDIR ], 0, 0);
$status = $zip->extractMember($memberName);
ok($status, AZ_OK);
ok(-d $dirName);

#--------- extract the directory by identity
ok(rmdir($dirName));	# it's still empty
$status = $zip->extractMember($member);
ok($status, AZ_OK);
ok(-d $dirName);

#--------- add a string member, uncompressed
$memberName = TESTDIR . '/string.txt';
# addString	# Archive::Zip::Archive
# newFromString	# Archive::Zip::Member
$member = $zip->addString(TESTSTRING, $memberName);
ok(defined($member));

ok($member->fileName(), $memberName);

# members	# Archive::Zip::Archive
@members = $zip->members();
ok(scalar(@members), 2);
ok($members[1], $member);

# numberOfMembers	# Archive::Zip::Archive
$numberOfMembers = $zip->numberOfMembers();
ok($numberOfMembers, 2);

# writeToFileNamed	# Archive::Zip::Archive
$status = $zip->writeToFileNamed( OUTPUTZIP );
ok($status, AZ_OK);

($status, $zipout) = testZip();
# STDERR->print("status= $status, out=$zipout\n");
skip($testZipDoesntWork, $status, 0);

ok($member->crc32(), TESTSTRINGCRC);

ok($member->crc32String(), sprintf("%08x", TESTSTRINGCRC));

#--------- extract it by name
$status = $zip->extractMember($memberName);
ok($status, AZ_OK);
ok(-f $memberName);
ok(fileCRC($memberName), TESTSTRINGCRC);

#--------- now compress it and re-test
my $oldCompressionMethod = 
	$member->desiredCompressionMethod(COMPRESSION_DEFLATED);
ok($oldCompressionMethod, COMPRESSION_STORED, 'old compression method OK');

# writeToFileNamed	# Archive::Zip::Archive
$status = $zip->writeToFileNamed( OUTPUTZIP );
ok($status, AZ_OK, 'writeToFileNamed returns AZ_OK');
ok($member->crc32(), TESTSTRINGCRC);
ok($member->uncompressedSize(), TESTSTRINGLENGTH);

($status, $zipout) = testZip();
# STDERR->print("status= $status, out=$zipout\n");
skip($testZipDoesntWork, $status, 0);

#--------- extract it by name
$status = $zip->extractMember($memberName);
ok($status, AZ_OK);
ok(-f $memberName);
ok(fileCRC($memberName), TESTSTRINGCRC);

#--------- add a file member, compressed
ok(rename($memberName, TESTDIR . '/file.txt'));
$memberName = TESTDIR . '/file.txt';

# addFile	# Archive::Zip::Archive
# newFromFile	# Archive::Zip::Member
$member = $zip->addFile($memberName);
ok(defined($member));

ok($member->desiredCompressionMethod(), COMPRESSION_DEFLATED);

# writeToFileNamed	# Archive::Zip::Archive
$status = $zip->writeToFileNamed( OUTPUTZIP );
ok($status, AZ_OK);
ok($member->crc32(), TESTSTRINGCRC);
ok($member->uncompressedSize(), TESTSTRINGLENGTH);

($status, $zipout) = testZip();
# STDERR->print("status= $status, out=$zipout\n");
skip($testZipDoesntWork, $status, 0);

#--------- extract it by name (note we have to rename it first
#--------- or we will clobber the original file
my $newName = $memberName;
$newName =~ s/\.txt/2.txt/;
$status = $zip->extractMember($memberName, $newName);
ok($status, AZ_OK);
ok(-f $newName);
ok(fileCRC($newName), TESTSTRINGCRC);

#--------- now make it uncompressed and re-test
$oldCompressionMethod =
	$member->desiredCompressionMethod(COMPRESSION_STORED);

ok($oldCompressionMethod, COMPRESSION_DEFLATED);

# writeToFileNamed	# Archive::Zip::Archive
$status = $zip->writeToFileNamed( OUTPUTZIP );
ok($status, AZ_OK);
ok($member->crc32(), TESTSTRINGCRC);
ok($member->uncompressedSize(), TESTSTRINGLENGTH);

($status, $zipout) = testZip();
# STDERR->print("status= $status, out=$zipout\n");
skip($testZipDoesntWork, $status, 0);

#--------- extract it by name
$status = $zip->extractMember($memberName, $newName);
ok($status, AZ_OK);
ok(-f $newName);
ok(fileCRC($newName), TESTSTRINGCRC);

# Now, the contents of OUTPUTZIP are:
# Length   Method    Size  Ratio   Date   Time   CRC-32    Name
#--------  ------  ------- -----   ----   ----   ------    ----
#       0  Stored        0   0%  03-17-00 11:16  00000000  testDir/
#     300  Defl:N      146  51%  03-17-00 11:16  ac373f32  testDir/string.txt
#     300  Stored      300   0%  03-17-00 11:16  ac373f32  testDir/file.txt
#--------          -------  ---                            -------
#     600              446  26%                            3 files

# members	# Archive::Zip::Archive
@members = $zip->members();
ok(scalar(@members), 3);
ok($members[2], $member);

# memberNames	# Archive::Zip::Archive
@memberNames = $zip->memberNames();
ok(scalar(@memberNames), 3);
ok($memberNames[2], $memberName);

# memberNamed	# Archive::Zip::Archive
ok($zip->memberNamed($memberName), $member);

# membersMatching	# Archive::Zip::Archive
@members = $zip->membersMatching('file');
ok(scalar(@members), 1);
ok($members[0], $member);

@members = $zip->membersMatching('.txt$');
ok(scalar(@members), 2);
ok($members[1], $member);

#--------- remove the string member and test the file
# removeMember	# Archive::Zip::Archive
$member = $zip->removeMember($members[0]);
ok($member, $members[0]);

$status = $zip->writeToFileNamed( OUTPUTZIP );
ok($status, AZ_OK);

($status, $zipout) = testZip();
# STDERR->print("status= $status, out=$zipout\n");
skip($testZipDoesntWork, $status, 0);

#--------- add the string member at the end and test the file
# addMember	# Archive::Zip::Archive
$zip->addMember($member);
@members = $zip->members();

ok(scalar(@members), 3);
ok($members[2], $member);

# memberNames	# Archive::Zip::Archive
@memberNames = $zip->memberNames();
ok(scalar(@memberNames), 3);
ok($memberNames[1], $memberName);

$status = $zip->writeToFileNamed( OUTPUTZIP );
ok($status, AZ_OK);

($status, $zipout) = testZip();
# STDERR->print("status= $status, out=$zipout\n");
skip($testZipDoesntWork, $status, 0);

#--------- remove the file member
$member = $zip->removeMember($members[1]);
ok($member, $members[1]);
ok($zip->numberOfMembers(), 2);

#--------- replace the string member with the file member
# replaceMember	# Archive::Zip::Archive
$member = $zip->replaceMember($members[2], $member);
ok($member, $members[2]);
ok($zip->numberOfMembers(), 2);

#--------- re-add the string member
$zip->addMember($member);
ok($zip->numberOfMembers(), 3);

@members = $zip->members();
$status = $zip->writeToFileNamed( OUTPUTZIP );
ok($status, AZ_OK);

($status, $zipout) = testZip();
# STDERR->print("status= $status, out=$zipout\n");
skip($testZipDoesntWork, $status, 0);

#--------- add compressed file
$member = $zip->addFile(File::Spec->catfile(TESTDIR, 'file.txt'));
ok(defined($member));
$member->desiredCompressionMethod(COMPRESSION_DEFLATED);
$member->fileName(TESTDIR . '/fileC.txt');

#--------- add uncompressed string
$member = $zip->addString(TESTSTRING, TESTDIR . '/stringU.txt');
ok(defined($member));
$member->desiredCompressionMethod(COMPRESSION_STORED);

# Now, the file looks like this:
# Length   Method    Size  Ratio   Date   Time   CRC-32    Name
#--------  ------  ------- -----   ----   ----   ------    ----
#       0  Stored        0   0%  03-17-00 12:30  00000000  testDir/
#     300  Stored      300   0%  03-17-00 12:30  ac373f32  testDir/file.txt
#     300  Defl:N      146  51%  03-17-00 12:30  ac373f32  testDir/string.txt
#     300  Stored      300   0%  03-17-00 12:30  ac373f32  testDir/stringU.txt
#     300  Defl:N      146  51%  03-17-00 12:30  ac373f32  testDir/fileC.txt
#--------          -------  ---                            -------
#    1200              892  26%                            5 files

@members = $zip->members();
$numberOfMembers = $zip->numberOfMembers();
ok($numberOfMembers, 5);

#--------- make sure the contents of the stored file member are OK.
# contents	# Archive::Zip::Archive
ok($zip->contents($members[1]), TESTSTRING);

# contents	# Archive::Zip::Member
ok($members[1]->contents(), TESTSTRING);

#--------- make sure the contents of the compressed string member are OK.
ok($members[2]->contents(), TESTSTRING);

#--------- make sure the contents of the stored string member are OK.
ok($members[3]->contents(), TESTSTRING);

#--------- make sure the contents of the compressed file member are OK.
ok($members[4]->contents(), TESTSTRING);

#--------- write to INPUTZIP
$status = $zip->writeToFileNamed( INPUTZIP );
ok($status, AZ_OK);

($status, $zipout) = testZip(INPUTZIP);
# STDERR->print("status= $status, out=$zipout\n");
skip($testZipDoesntWork, $status, 0);

#--------- read from INPUTZIP (appending its entries)
# read	# Archive::Zip::Archive
$status = $zip->read(INPUTZIP);
ok($status, AZ_OK);
ok($zip->numberOfMembers(), 10);

#--------- clean up duplicate names
@members = $zip->members();
$member = $zip->removeMember($members[5]);
ok($member->fileName(), TESTDIR . '/');

{
	for my $i (6..9)
	{
		$memberName = $members[$i]->fileName();
		$memberName =~ s/\.txt/2.txt/;
		$members[$i]->fileName($memberName);
	}
}
ok(scalar($zip->membersMatching('2.txt')), 4);

#--------- write zip out and test it.
$status = $zip->writeToFileNamed( OUTPUTZIP );
ok($status, AZ_OK);

($status, $zipout) = testZip();
# STDERR->print("status= $status, out=$zipout\n");
skip($testZipDoesntWork, $status, 0);

#--------- Make sure that we haven't renamed files (this happened!)
ok(scalar($zip->membersMatching('2\.txt$')), 4);

#--------- Now try extracting everyone
@members = $zip->members();
ok($zip->extractMember($members[0]), AZ_OK);	#DM
ok($zip->extractMember($members[1]), AZ_OK);	#NFM
ok($zip->extractMember($members[2]), AZ_OK);
ok($zip->extractMember($members[3]), AZ_OK);	#NFM
ok($zip->extractMember($members[4]), AZ_OK);
ok($zip->extractMember($members[5]), AZ_OK);
ok($zip->extractMember($members[6]), AZ_OK);
ok($zip->extractMember($members[7]), AZ_OK);
ok($zip->extractMember($members[8]), AZ_OK);

#--------- count dirs
{
	my @dirs = grep { $_->isDirectory() } @members;
	ok(scalar(@dirs), 1); 
	ok($dirs[0], $members[0]);
}

#--------- count binary and text files
{
	my @binaryFiles = grep { $_->isBinaryFile() } @members;
	my @textFiles = grep { $_->isTextFile() } @members;
	ok(scalar(@binaryFiles), 5); 
	ok(scalar(@textFiles), 4); 
}

#--------- Try writing zip file to file handle
{
	my $fh;
	if ($catWorks)
	{
		unlink( OUTPUTZIP );
		$fh = FileHandle->new( CATPIPE . OUTPUTZIP );
		binmode($fh);
	}
	skip(!$catWorks, $fh);
#	$status = $zip->writeToFileHandle($fh, 0) if ($catWorks);
	$status = $zip->writeToFileHandle($fh) if ($catWorks);
	skip(!$catWorks, $status, AZ_OK);
	$fh->close() if ($catWorks);
	($status, $zipout) = testZip();
	ok($status, 0);
}

#--------- Change the contents of a string member
ok(ref($members[2]), 'Archive::Zip::StringMember');
$members[2]->contents( "This is my new contents\n" );

#--------- write zip out and test it.
$status = $zip->writeToFileNamed( OUTPUTZIP );
ok($status, AZ_OK);

($status, $zipout) = testZip();
# STDERR->print("status= $status, out=$zipout\n");
skip($testZipDoesntWork, $status, 0);

#--------- Change the contents of a file member
ok(ref($members[1]), 'Archive::Zip::NewFileMember');
$members[1]->contents( "This is my new contents\n" );

#--------- write zip out and test it.
$status = $zip->writeToFileNamed( OUTPUTZIP );
ok($status, AZ_OK);

($status, $zipout) = testZip();
# STDERR->print("status= $status, out=$zipout\n");
skip($testZipDoesntWork, $status, 0);

#--------- Change the contents of a zip member

ok(ref($members[7]), 'Archive::Zip::ZipFileMember');
$members[7]->contents( "This is my new contents\n" );

#--------- write zip out and test it.
$status = $zip->writeToFileNamed( OUTPUTZIP );
ok($status, AZ_OK);

($status, $zipout) = testZip();
# STDERR->print("status= $status, out=$zipout\n");
skip($testZipDoesntWork, $status, 0);


#--------- now clean up
# END { system("rm -rf " . TESTDIR . " " . OUTPUTZIP . " " . INPUTZIP) }

#--------------------- STILL UNTESTED IN THIS SCRIPT --------------------- 

# sub setChunkSize	# Archive::Zip
# sub _formatError	# Archive::Zip
# sub _error	# Archive::Zip
# sub _subclassResponsibility 	# Archive::Zip
# sub diskNumber	# Archive::Zip::Archive
# sub diskNumberWithStartOfCentralDirectory	# Archive::Zip::Archive
# sub numberOfCentralDirectoriesOnThisDisk	# Archive::Zip::Archive
# sub numberOfCentralDirectories	# Archive::Zip::Archive
# sub centralDirectoryOffsetWRTStartingDiskNumber	# Archive::Zip::Archive
# sub extraField	# Archive::Zip::Member
# sub isEncrypted	# Archive::Zip::Member
# sub isTextFile	# Archive::Zip::Member
# sub isBinaryFile	# Archive::Zip::Member
# sub isDirectory	# Archive::Zip::Member
# sub lastModTime	# Archive::Zip::Member
# sub _dosToUnixTime	# Archive::Zip::Member
# sub _writeDataDescriptor	# Archive::Zip::Member
# sub isDirectory	# Archive::Zip::DirectoryMember
# sub _becomeDirectory	# Archive::Zip::DirectoryMember
# sub diskNumberStart	# Archive::Zip::ZipFileMember
