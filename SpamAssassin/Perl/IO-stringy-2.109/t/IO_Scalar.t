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

my $RECORDSEP_TESTS = 'undef empty custom newline';
sub opener { my $s = join('', @{$_[0]}); IO::Scalar->new(\$s); }

### Make a tester:
my $T = typical ExtUtils::TBone;
Common->test_init(TBone=>$T);
$T->log_warnings;

### Set the counter:
my $main_tests = 1 + 1;
my $common_tests = (1 + 1 + 4 + 4 + 3 + 4
		    + Common->test_recordsep_count($RECORDSEP_TESTS));
$T->begin($main_tests + $common_tests);

### Open a scalar on a string, containing initial data:
my $s = $Common::DATA_S;
my $SH = IO::Scalar->new(\$s);
$T->ok($SH, "OPEN: open a scalar on a ref to a string");

### Run standard tests:
Common->test_print($SH);
$T->ok(($s eq $Common::FDATA_S), "FULL",
       S=>$s, F=>$Common::FDATA_S);
Common->test_getc($SH);
Common->test_getline($SH);
Common->test_read($SH);
Common->test_seek($SH);
Common->test_tie(TieArgs => ['IO::Scalar']);
Common->test_recordsep($RECORDSEP_TESTS, \&opener);

### So we know everything went well...
$T->end;


