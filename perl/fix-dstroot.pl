#!/usr/bin/perl

use strict;
use Config ();

sub fixConfig {
    my($dstroot, $archname) = @_;
    local $_;
    my $file = "$archname/Config.pm";
    my $tmp = "$file.tmp$$";
    open(FROM, $file) or die "Can't open $file: $!\n";
    open(TO, ">$tmp") or die "Can't open $file: $!\n";
    while(<FROM>) {
	s/$dstroot//g;
	s/-arch\s+\S+\s*//g;
	print TO $_;
    }
    close(TO);
    close(FROM);
    rename($tmp, $file) or die "rename($tmp, $file): $!\n";
}

sub fixPacklist {
    my($dstroot, $archname) = @_;
    local $_;
    my $file = "$archname/.packlist";
    my $tmp = "$file.tmp$$";
    open(FROM, $file) or die "Can't open $file: $!\n";
    open(TO, ">$tmp") or die "Can't open $file: $!\n";
    while(<FROM>) {
	s/$dstroot//g;
	print TO $_;
    }
    close(TO);
    close(FROM);
    rename($tmp, $file) or die "rename($tmp, $file): $!\n";
}

exit 1 if scalar(@ARGV) != 1;
my $dstroot = $ARGV[0];
my $archname = $Config::Config{archname};
my $version = sprintf "%vd", $^V;
my $versiondir = "$dstroot/System/Library/Perl/$version";

chdir($versiondir) or die "Can't chdir($versiondir): $!\n";
$dstroot =~ s/[][\.,*^\$\@+\\(){}]/\\$&/g; # escape magic characters
fixConfig($dstroot, $archname);
fixPacklist($dstroot, $archname);
