#!/usr/bin/perl -w         #-*-Perl-*-

use lib "./t", "./lib"; 
use IO::ScalarArray;
use ExtUtils::TBone;
use Common;


#--------------------
#
# TEST...
#
#--------------------

my $RECORDSEP_TESTS = 'undef newline';
sub opener { my $a = [@{$_[0]}]; IO::ScalarArray->new($a); }

# Make a tester:
my $T = typical ExtUtils::TBone;
Common->test_init(TBone=>$T);

# Set the counter:
my $main_tests = 1;
my $common_tests = (1 + 1 + 4 + 4 + 3 + 4
		    + Common->test_recordsep_count($RECORDSEP_TESTS));
$T->begin($main_tests + $common_tests);

# Open a scalar on a string, containing initial data:
my @sa = @Common::DATA_SA;
my $SAH = IO::ScalarArray->new(\@sa);
$T->ok($SAH, "OPEN: open a scalar on a ref to an array");

# Run standard tests:
Common->test_print($SAH);
Common->test_getc($SAH);
Common->test_getline($SAH);
Common->test_read($SAH);
Common->test_seek($SAH);
Common->test_tie(TieArgs => ['IO::ScalarArray', []]);
Common->test_recordsep($RECORDSEP_TESTS, \&opener);

# So we know everything went well...
$T->end;








