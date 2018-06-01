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
    "10.10.2",
    "10.10.3",
    "10.11",
    "10.11.2",
    "10.11.3",
    "10.11.4",
    "10.12",
    "10.12.1",
    "10.12.2",
    "10.12.4",
    "10.13",
    "10.13.1",
    "10.13.2",
    "10.13.4",
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
    "8.1",
    "8.2",
    "8.3",
    "8.4",
    "9.0",
    "9.1",
    "9.2",
    "9.3",
    "10.0",
    "10.1",
    "10.2",
    "10.3",
    "11.0",
    "11.1",
    "11.2",
    "11.3",
    "11.4",
);

my @appletvos_versions = (
    "9.0",
    "9.1",
    "9.2",
    "10.0",
    "10.0.1",
    "10.1",
    "10.2",
    "11.0",
    "11.1",
    "11.2",
    "11.3",
    "11.4",
);

my @watchos_versions = (
    "1.0",
    "2.0",
    "2.1",
    "2.2",
    "3.0",
    "3.1",
    "3.1.1",
    "3.2",
    "4.0",
    "4.1",
    "4.2",
    "4.3",
);

my @bridgeos_versions = (
    "2.0",
);

my $m;
my $i;
my $a;
my $w;
my $b;
GetOptions('macosx' => \$m, 'osx' => \$m, 'ios' => \$i, 'appletvos' => \$a, 'watchos' => \$w, 'bridgeos' => \$b);

if ($m) {
  print join(" ", @macosx_versions) . "\n";
} elsif ($i) {
  print join(" ", @ios_versions) . "\n";
} elsif ($a) {
  print join(" ", @appletvos_versions) . "\n";
} elsif ($w) {
  print join(" ", @watchos_versions) . "\n";
} elsif ($b) {
  print join(" ", @bridgeos_versions) . "\n";
} else {
  print "usage: $0 --macosx|--osx|--ios|--appletvos|--watchos|--bridgeos\n";
}

