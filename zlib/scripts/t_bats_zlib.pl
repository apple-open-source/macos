#!/usr/bin/perl -w
# BATS entry point for zlib tests

use File::Basename;

# Get our location
my $scripts_dir = dirname(__FILE__);

# Figure out which code to run
my $uname = `uname -m -p`;
my $bindir = 'osx';
$bindir = 'ios' if ( $uname =~ /arm/ );
$bindir = 'wos' if ( $uname =~ /Watch/ );
$bindir = $scripts_dir.'/../bin/'.$bindir;
my $datadir = $scripts_dir.'/../data';

if ( not -d $bindir ) {
  print STDERR "ERROR: binary directory not found: $bindir\n";
  exit(1);
}
if ( not -d $datadir ) {
  print STDERR "ERROR: binary directory not found: $datadir\n";
  exit(1);
}

my $cmd = "$scripts_dir/t_zlib_all.pl -d \"$datadir\" -b $bindir/t_zlib";
system($cmd);

exit(1) if ( $? != 0 ); # failed
exit(0); #OK
