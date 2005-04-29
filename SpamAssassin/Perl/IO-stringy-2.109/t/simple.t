#!/usr/bin/perl -w         #-*-Perl-*-

use lib "./t", "./lib"; 
use IO::Scalar;
use IO::ScalarArray;
use IO::Lines;
use ExtUtils::TBone;
use Common;


#--------------------
#
# TEST...
#
#--------------------

### Make a tester:
my $T = typical ExtUtils::TBone;
Common->test_init(TBone=>$T);
$T->log_warnings;

### Set the counter:
my $ntests = 6;
$T->begin($ntests);

#------------------------------

my $SH = new IO::Scalar;
print $SH "Hi there!\n";
print $SH "Tres cool, no?\n";
$T->ok_eq(${$SH->sref}, "Hi there!\nTres cool, no?\n");    

$SH->seek(0, 0);
my $line = <$SH>;
$T->ok_eq($line, "Hi there!\n");

#------------------------------

my $AH = new IO::ScalarArray;
print $AH "Hi there!\n";
print $AH "Tres cool, no?\n";
$T->ok_eq(join('', @{$AH->aref}), "Hi there!\nTres cool, no?\n");    

$AH->seek(0, 0);
$line = <$AH>;
$T->ok_eq($line, "Hi there!\n");

#------------------------------

my $LH = new IO::Lines;
print $LH "Hi there!\n";
print $LH "Tres cool, no?\n";
$T->ok_eq(join('', @{$LH->aref}), "Hi there!\nTres cool, no?\n");    

$LH->seek(0, 0);
$line = <$LH>;
$T->ok_eq($line, "Hi there!\n");



### So we know everything went well...
$T->end;

