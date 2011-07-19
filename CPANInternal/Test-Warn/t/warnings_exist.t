#!/usr/bin/perl

use strict;
use warnings;

use Carp;
use Test::More qw(no_plan);

my $file="t/warnings_exist1.pl";
my $output=`$^X -Mblib $file 2>&1`;
$output=~s/^#.*$//gm;
$output=~s/\n{2,}/\n/gs;
my @lines=split /[\n\r]+/,$output;
shift @lines if $lines[0]=~/^Using /; #extra line in perl 5.6.2
#print $output;
my @expected=(
"warn_2 at $file line 12.",
'ok 1',
'ok 2',
"warn_2 at $file line 21.",
'not ok 3',
"warn_2 at $file line 27.",
'ok 4',
"warn_2 at $file line 31.",
'not ok 5',
qr/^Use of uninitialized value (?:\$a\s+)?in addition \(\+\) at \Q$file\E line 36\.$/,
'ok 6',
'1..6'
);
foreach my $i (0..$#expected) {
  if ($expected[$i]=~/^\(\?\w*-\w*:/) {
    like($lines[$i],$expected[$i]);
  } else {
    is($lines[$i],$expected[$i]);
  }
}
