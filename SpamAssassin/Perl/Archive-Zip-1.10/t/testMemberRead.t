# Test Archive::Zip::MemberRead module
# $Revision: 1.1 $
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl t/testMemberRead.t'
# vim: ts=4 sw=4 ft=perl

$^W = 1;
use strict;
use Test;
use Archive::Zip qw( :ERROR_CODES :CONSTANTS );
use Archive::Zip::MemberRead;

BEGIN { plan tests => 7, todo => [] }
BEGIN { require 't/common.pl' }
use constant FILENAME => File::Spec->catfile(TESTDIR, 'member_read.zip');

my ($zip, $member, $fh, @data);
$zip  = new Archive::Zip;
@data = ( 'Line 1', 'Line 2', '', 'Line 3', 'Line 4' );

$zip->addString(join("\n", @data), 'string.txt');
$zip->writeToFileNamed(FILENAME);

$member = $zip->memberNamed('string.txt');
$fh     = $member->readFileHandle();
ok( $fh );

my ($line, $not_ok, $ret, $buffer);
while (defined($line = $fh->getline()))
{
	$not_ok = 1 if ($line ne $data[$fh->input_line_number()-1]);
}
ok( !$not_ok );

$fh->rewind();
$ret = $fh->read($buffer, length($data[0]));
ok( $ret == length($data[0]) );
ok( $buffer eq $data[0] );
$fh->close();

#
# Different usages 
#
$fh = new Archive::Zip::MemberRead($zip, 'string.txt');
ok($fh);

$fh = new Archive::Zip::MemberRead($zip, $zip->memberNamed('string.txt'));
ok($fh);

$fh = new Archive::Zip::MemberRead($zip->memberNamed('string.txt'));
ok($fh);
