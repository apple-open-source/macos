# Blocking Exclusive Lock Test

use strict;
use warnings;

use Test::More;
if( $^O eq 'MSWin32' ) {
  plan skip_all => 'Tests fail on Win32 due to forking';
}
else {
  plan tests => 20+2;
}
use File::NFSLock;
use Fcntl qw(O_CREAT O_RDWR O_RDONLY O_TRUNC LOCK_EX);

# $m simultaneous processes each trying to count to $n
my $m = 20;
my $n = 50;

$| = 1; # Buffer must be autoflushed because of fork() below.

my $datafile = "testfile.dat";

# Create a blank file
sysopen ( my $fh, $datafile, O_CREAT | O_RDWR | O_TRUNC );
close ($fh);
ok (-e $datafile && !-s _);

for (my $i = 0; $i < $m ; $i++) {
  # For each process
  if (!fork) {
    # Child process need to count to $n
    for (my $j = 0; $j < $n ; $j++) {
      my $lock = new File::NFSLock {
        file => $datafile,
        lock_type => LOCK_EX,
      };
      sysopen(my $fh, $datafile, O_RDWR);
      # Read the current value
      my $count = <$fh>;
      # Increment it
      $count ++;
      # And put it back
      seek ($fh,0,0);
      print $fh "$count\n";
      close $fh;
    }
    exit;
  }
}

for (my $i = 0; $i < $m ; $i++) {
  # Wait until all the children are finished counting
  wait;
  ok 1;
}

# Load up whatever the file says now
sysopen(my $fh2, $datafile, O_RDONLY);
$_ = <$fh2>;
close $fh2;
chomp;
# It should be $m processes time $n each
is $n*$m, $_;

# Wipe the temporary file
unlink $datafile;
