#!/usr/bin/perl -w
# Install SDK/runtime roots
# EB Jan 2020

# Split the contents of -d dir between runtime contents and SDK contents, then
# darwinup the runtime to / and the SDK to the specified -s sdk

use strict;
use warnings;
use Cwd qw/ realpath /;
use Getopt::Std;
use File::Temp qw/ tempdir /;
use File::Basename;

our $opt_d = ''; # -d dstroot
our $opt_s = ''; # -s sdk
our $opt_o = ''; # -o archive (optional)
our $opt_v = 1;
our $opt_r = 0;  # -r = install runtime, default is install SDK
my $tmp_runtime = tempdir( CLEANUP => 1 );
my $tmp_sdk = tempdir( CLEANUP => 1 );
my $usage = <<'EOF';
Usage: t_install_roots.pl -d dstroot -s sdk [-v]
EOF

getopts('vrd:s:o:');
my $dstroot = realpath($opt_d);
unless ( -d $dstroot ) { print $usage; exit(1); }
my $sdkroot = `xcrun --sdk $opt_s -show-sdk-path`;
chop($sdkroot);
unless ( -d $sdkroot ) { print "Invalid sdk: $opt_s\n"; print $usage; exit(1); }

my $action = "Installing";
$action = "Archiving" if ($opt_o ne '');
my $roots = "SDK";
$roots = "Runtime" if ($opt_r);

if ($opt_v) {
  print STDERR "Processing roots from: $dstroot\n";
  print STDERR "Target SDK $opt_s -> $sdkroot\n";
  print STDERR "$action $roots\n";
}

open(FILE,"find $dstroot -type f -o -type l |") or die "Running find";
F: while (<FILE>) {
  chop;
  my $full_path = $_;
  my $path = $full_path;
  $path =~ s/\Q$dstroot\/\E//;
  my $is_runtime = 0;
  my $is_sdk = 0;
  my $file_info = `file $full_path`;
  $is_runtime = 1 if ($path =~ /\.dylib$/);
  $is_sdk = 1 if ($path =~ /\.h$/);
  $is_sdk = 1 if ($path =~ /lib.*\.a$/);
  $is_sdk = 1 if ($path =~ /module.*modulemap$/);
  $is_sdk = 1 if ($path =~ /\.tbd$/);
  $is_sdk = 1 if ($path =~ /\.swiftmodule\//);
  $is_sdk = 1 if ($path =~ /\/OpenSource/);
  $is_runtime = 1 if (!$is_sdk && $file_info =~ /Mach-O.*binary/);
  $is_runtime = 1 if ($path =~ /\/man\/man.\//);
  die "File not labelled: $path" if ($is_runtime == $is_sdk);
  if ($opt_v) {
    print STDERR $is_runtime?"RUNTIME":"SDK    ";
    print STDERR " $path\n";
  }
  my $dpath;
  if ($is_runtime) {
    $dpath = dirname("$tmp_runtime/$path");
  } else {
    $dpath = dirname("$tmp_sdk/$path");
  }
  system("mkdir -p $dpath");
  system("cp -af \"$full_path\" \"$dpath\"");
}

my $darwinup = "sudo darwinup -f";
$darwinup .= " -v" if ($opt_v);

# If -o is specified, archive instead of darwinup
if ($opt_o ne '') {
  print STDERR "Archiving $roots to $opt_o...\n";
  if ($opt_r) {
    system("tar cvzf \"$opt_o\" -C $tmp_runtime .");
  } else {
    system("tar cvzf \"$opt_o\" -C $tmp_sdk .");
  }
} else {
  # Always print this before the script asks for a password
  print STDERR "Running darwinup $roots...\n";
  if ($opt_r) {
    system("$darwinup install $tmp_runtime");
  } else {
    system("$darwinup -p \"$sdkroot\" install $tmp_sdk");
  }
}

exit(0);
