# Blocking Exclusive test within a single process (no fork)

use Test;
use File::NFSLock;
use Fcntl qw(O_CREAT O_RDWR O_RDONLY O_TRUNC LOCK_EX);

plan tests => 3;

# Everything loaded fine
ok (1);

my $datafile = "testfile.dat";

# Create a blank file
sysopen ( FH, $datafile, O_CREAT | O_RDWR | O_TRUNC );
close (FH);
ok (-e $datafile && !-s _);
# Wipe any old stale locks
unlink "$datafile$File::NFSLock::LOCK_EXTENSION";

# Single process trying to count to $n
my $n = 20;

for (my $i = 0; $i < $n ; $i++) {
  my $lock = new File::NFSLock {
    file => $datafile,
    lock_type => LOCK_EX,
  };
  sysopen(FH, $datafile, O_RDWR);

  # Read the current value
  my $count = <FH>;
  # Increment it
  $count ++;

  # And put it back
  seek (FH,0,0);
  print FH "$count\n";
  close FH;
}

# Load up whatever the file says now
sysopen(FH, $datafile, O_RDONLY);
$_ = <FH>;
close FH;
chomp;
# It should be the same as the number of times it looped
ok $n, $_;

# Wipe the temporary file
unlink $datafile;
