#!/usr/bin/perl -w         #-*-Perl-*-

use lib "./t", "./lib"; 
use IO::Scalar;
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
my $ntests = (($] >= 5.004) ? 2 : 0);
$T->begin($ntests);
if ($ntests == 0) {
    $T->end;
    exit 0;
}

### Open handles on strings:
my $str1 = "Tea for two";
my $str2 = "Me 4 U";
my $str3 = "hello";
my $S1 = IO::Scalar->new(\$str1);
my $S2 = IO::Scalar->new(\$str2);

### Interleave output:
print $S1 ", and two ";
print $S2 ", and U ";
my $S3 = IO::Scalar->new(\$str3);
$S3->print(", world");
print $S1 "for tea";
print $S2 "4 me";

### Verify:
$T->ok_eq($str1, 
	  "Tea for two, and two for tea",
	  "COHERENT STRING 1");
$T->ok_eq($str2, 
	  "Me 4 U, and U 4 me",
	  "COHERENT STRING 2");

### So we know everything went well...
$T->end;

