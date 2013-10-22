# Lock Test with fatal error (die)

use strict;
use warnings;

use Test::More tests => 9;
use File::NFSLock;
use Fcntl qw(O_CREAT O_RDWR O_RDONLY O_TRUNC LOCK_EX);

$| = 1; # Buffer must be autoflushed because of fork() below.

my $datafile = "testfile.dat";

# Wipe lock file in case it exists
unlink ("$datafile$File::NFSLock::LOCK_EXTENSION");

# Create a blank file
sysopen ( my $fh, $datafile, O_CREAT | O_RDWR | O_TRUNC );
close ($fh);
# test 1
ok (-e $datafile && !-s _);


# test 2
my ($rd1, $wr1);
ok (pipe($rd1, $wr1)); # Connected pipe for child1

my $pid = fork;
if (!$pid) {
  # Child #1 process
  # Obtain exclusive lock
  my $lock = new File::NFSLock {
    file => $datafile,
    lock_type => LOCK_EX,
  };
  print $wr1 !!$lock; # Send boolean success status down pipe
  close($wr1); # Signal to parent that the Blocking lock is done
  close($wr1);
  if ($lock) {
    sysopen(my $fh, $datafile, O_RDWR | O_TRUNC);
    # And then put a magic word into the file
    print $fh "exclusive\n";
    close $fh;
    open(STDERR,">/dev/null");
    die "I will die while lock is still aquired";
  }
  die "Lock failed!";
}

# test 3
ok 1; # Fork successful
close ($wr1);
# Waiting for child1 to finish its lock status
my $child1_lock = <$rd1>;
close ($rd1);
# Report status of the child1_lock.
# It should have been successful
# test 4
ok ($child1_lock);

# Clear the zombie
# test 5
ok (wait);

# test 6
my ($rd2, $wr2);
ok (pipe($rd2, $wr2)); # Connected pipe for child2
if (!fork) {
  # The last lock died, so this should aquire fine.
  my $lock = new File::NFSLock {
    file => $datafile,
    lock_type => LOCK_EX,
    blocking_timeout => 10,
  };
  if ($lock) {
    sysopen(my $fh, $datafile, O_RDWR | O_TRUNC);
    # Immediately put the magic word into the file
    print $fh "lock2\n";
    truncate ($fh, tell $fh);
    close $fh;
  }
  print $wr2 !!$lock; # Send boolean success status down pipe
  close($wr2); # Signal to parent that the Blocking lock is done
  close($rd2);
  exit; # Release this new lock
}
# test 7
ok 1; # Fork successful
close ($wr2);

# Waiting for child2 to finish its lock status
my $child2_lock = <$rd2>;
close ($rd2);
# Report status of the child2_lock.
# This should have been successful.
# test 8
ok ($child2_lock);

# Load up whatever the file says now
sysopen(my $fh2, $datafile, O_RDONLY);

$_ = <$fh2>;
# test 9
ok /lock2/;
close $fh2;

# Wipe the temporary file
unlink $datafile;
