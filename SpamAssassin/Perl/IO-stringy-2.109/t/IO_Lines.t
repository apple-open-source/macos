#!/usr/bin/perl -w         #-*-Perl-*-

use lib "./t", "./lib"; 
use IO::Lines;
use ExtUtils::TBone;
use Common;


#--------------------
#
# TEST...
#
#--------------------

my $RECORDSEP_TESTS = 'undef newline';
sub opener { my $a = [@{$_[0]}]; IO::Lines->new($a); }

# Make a tester:
my $T = typical ExtUtils::TBone;
Common->test_init(TBone=>$T);

# Set the counter:
my $main_tests = 1;
my $common_tests = (1 + 1 + 4 + 4 + 3 + 4
		    + Common->test_recordsep_count($RECORDSEP_TESTS));
$T->begin($main_tests + $common_tests);

# Open a scalar on a string, containing initial data:
my @la = @Common::DATA_LA;
my $LAH = IO::Lines->new(\@la);
$T->ok($LAH, "OPEN: open a scalar on a ref to an array");

# Run standard tests:
Common->test_print($LAH);
Common->test_getc($LAH);
Common->test_getline($LAH);
Common->test_read($LAH);
Common->test_seek($LAH);
Common->test_tie(TieArgs => ['IO::Lines', []]);
Common->test_recordsep($RECORDSEP_TESTS, \&opener);

# So we know everything went well...
$T->end;


