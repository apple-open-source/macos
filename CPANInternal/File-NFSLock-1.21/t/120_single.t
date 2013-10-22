# Blocking Exclusive test within a single process (no fork)

use Test::More tests => 2;
use File::NFSLock;
use Fcntl qw(O_CREAT O_RDWR O_RDONLY O_TRUNC LOCK_EX);

my $datafile = "testfile.dat";

# Create a blank file
sysopen ( my $fh, $datafile, O_CREAT | O_RDWR | O_TRUNC );
close ($fh);
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

# Load up whatever the file says now
sysopen($fh, $datafile, O_RDONLY);
$_ = <$fh>;
close $fh;
chomp;
# It should be the same as the number of times it looped
is $n, $_;

# Wipe the temporary file
unlink $datafile;
