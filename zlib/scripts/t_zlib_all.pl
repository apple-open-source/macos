#!/usr/bin/perl -w
# Run t_zlib on all regular files in the specified directory


use Getopt::Std;
use File::Temp;

our $opt_d = '.';
our $opt_h = 0;
our $opt_b = '.';

getopts('hvsd:a:r:b:');

if ($opt_h) {
  print STDERR "Usage: t_zlib_all.pl [-h] [-d dataDir] [-b tester]\n";
  print STDERR "-h       print usage and quit\n";
  print STDERR "-d       directory providing test files\n";
  print STDERR "-b       path to tester\n";

  exit(1);
}
my $dataDir = $opt_d;
my $status = 0;
my $binFile = $opt_b;

open(FILE,"find \"$dataDir\" -type f|") or die "find";

LINE: while (my $f = <FILE>) {
  chop($f);
  next LINE if ( $f =~ /\.svn\// ); # skip subversion stuff
  next LINE if ( $f =~ /\.git\// ); # skip git stuff
  next LINE if ( $f =~ /\.DS_Store$/ ); # skip .DS_Store
  next LINE unless ( -f $f );
  my $sz = (-s $f);
    
  next LINE if ( $sz < 1 or $sz > (2 << 30)); # alloc will fail if too large
  my $cmd = $binFile.' "'.$f.'"';
  print STDERR "$cmd ";
  $status = system($cmd);
  exit(1) if ( $status != 0 ); # return 1 if failed
}

close(FILE);
exit(0); #OK
