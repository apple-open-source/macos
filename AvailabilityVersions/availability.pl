#!/usr/bin/perl

use strict;
use warnings;
use Getopt::Long;

my @macosx_versions = (
    "10.0",
    "10.1",
    "10.2",
    "10.3",
    "10.4",
    "10.5",
    "10.6",
    "10.7",
    "10.8",
    "10.9",
    "10.10",
);

my @ios_versions = (
    "2.0",
    "2.1",
    "2.2",
    "3.0",
    "3.1",
    "3.2",
    "4.0",
    "4.1",
    "4.2",
    "4.3",
    "5.0",
    "5.1",
    "6.0",
    "6.1",
    "7.0",
    "7.1",
    "8.0",
);

my $m;
my $i;
GetOptions('macosx' => \$m, 'ios' => \$i);

if ($m) {
  print join(" ", @macosx_versions) . "\n";
} elsif ($i) {
  print join(" ", @ios_versions) . "\n";
} else {
  print "usage: $0 --macosx|--ios\n";
}

