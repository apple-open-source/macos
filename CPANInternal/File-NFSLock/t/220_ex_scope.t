# Non-Blocking Exclusive Lock Scope Test
#
# This tests to make sure a failed lock leaving
# scope does not unlock a lock of someone else.
#
# Exploits the conditions found by Andy Hird (andyh@myinternet.com.au)
# Here are his comments:
#
# If a process has some file locked (say exclusively although it doesn't matter) and another process attempts to get a lock, if it fails it deletes the lock file - whether or not the first (locking process) has finished with its lock. This means any subsequent process that comes along that attempts to lock the file succeeds - even if the first process thinks it still has a lock.
#

use Test;
use File::NFSLock;
use Fcntl qw(O_CREAT O_RDWR O_RDONLY O_TRUNC LOCK_EX LOCK_NB);

$| = 1; # Buffer must be autoflushed because of fork() below.
plan tests => 11;

my $datafile = "testfile.dat";

# Create a blank file
sysopen ( FH, $datafile, O_CREAT | O_RDWR | O_TRUNC );
close (FH);
ok (-e $datafile && !-s _);


ok (pipe(RD1,WR1)); # Connected pipe for child1
if (!fork) {
  # Child #1 process
  my $lock = new File::NFSLock {
    file => $datafile,
    lock_type => LOCK_EX | LOCK_NB,
  };
  print WR1 !!$lock; # Send boolean success status down pipe
  close(WR1); # Signal to parent that the Non-Blocking lock is done
  close(RD1);
  if ($lock) {
    sleep 2;  # hold the lock for a moment
    sysopen(FH, $datafile, O_RDWR);
    # now put a magic word into the file
    print FH "child1\n";
    close FH;
  }
  exit;
}
ok 1; # Fork successful
close (WR1);
# Waiting for child1 to finish its lock status
my $child1_lock = <RD1>;
close (RD1);
# Report status of the child1_lock.
# It should have been successful
ok ($child1_lock);


ok (pipe(RD2,WR2)); # Connected pipe for child2
if (!fork) {
  # Child #2 process
  my $lock = new File::NFSLock {
    file => $datafile,
    lock_type => LOCK_EX | LOCK_NB,
  };
  print WR2 !!$lock; # Send boolean success status down pipe
  close(WR2); # Signal to parent that the Non-Blocking lock is done
  close(RD2);
  if ($lock) {
    sysopen(FH, $datafile, O_RDWR);
    # now put a magic word into the file
    print FH "child2\n";
    close FH;
  }
  exit;
}
ok 1; # Fork successful
close (WR2);
# Waiting for child2 to finish its lock status
my $child2_lock = <RD2>;
close (RD2);
# Report status of the child2_lock.
# This lock should not have been obtained since
# the child1 lock should still have been established.
ok (!$child2_lock);

ok (pipe(RD3,WR3)); # Connected pipe for child3
if (!fork) {
  # Child #3 process
  my $lock = new File::NFSLock {
    file => $datafile,
    lock_type => LOCK_EX | LOCK_NB,
  };
  print WR3 !!$lock; # Send boolean success status down pipe
  close(WR3); # Signal to parent that the Non-Blocking lock is done
  close(RD3);
  if ($lock) {
    sysopen(FH, $datafile, O_RDWR);
    # now put a magic word into the file
    print FH "child3\n";
    close FH;
  }
  exit;
}
ok 1; # Fork successful
close (WR3);
# Waiting for child2 to finish its lock status
my $child3_lock = <RD3>;
close (RD3);
# Report status of the child3_lock.
# This lock should also fail since the child1
# lock should still have been established.
ok (!$child3_lock);

# Wait until the children have finished.
wait; wait; wait;

# Load up whatever the file says now
sysopen(FH, $datafile, O_RDONLY);
$_ = <FH>;
close FH;

# It should be child1 if it was really nonblocking
# since it got the lock first.
ok /child1/;

# Wipe the temporary file
unlink $datafile;
