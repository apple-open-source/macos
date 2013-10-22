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

use strict;
use warnings;

use Test::More;
if( $^O eq 'MSWin32' ) {
  plan skip_all => 'Tests fail on Win32 due to forking';
}
else {
  plan tests => 11;
}
use File::NFSLock;
use Fcntl qw(O_CREAT O_RDWR O_RDONLY O_TRUNC LOCK_EX LOCK_NB);

$| = 1; # Buffer must be autoflushed because of fork() below.

my $datafile = "testfile.dat";

# Create a blank file
sysopen ( my $fh, $datafile, O_CREAT | O_RDWR | O_TRUNC );
close ($fh);
ok (-e $datafile && !-s _);


my ($rd1, $wr1);
ok (pipe($rd1, $wr1)); # Connected pipe for child1
if (!fork) {
  # Child #1 process
  my $lock = new File::NFSLock {
    file => $datafile,
    lock_type => LOCK_EX | LOCK_NB,
  };
  print $wr1 !!$lock; # Send boolean success status down pipe
  close($wr1); # Signal to parent that the Non-Blocking lock is done
  close($rd1);
  if ($lock) {
    sleep 2;  # hold the lock for a moment
    sysopen(my $fh, $datafile, O_RDWR);
    # now put a magic word into the file
    print $fh "child1\n";
    close $fh;
  }
  exit;
}
ok 1; # Fork successful
close ($wr1);
# Waiting for child1 to finish its lock status
my $child1_lock = <$rd1>;
close ($rd1);
# Report status of the child1_lock.
# It should have been successful
ok ($child1_lock);


my ($rd2, $wr2);
ok (pipe($rd2, $wr2)); # Connected pipe for child2
if (!fork) {
  # Child #2 process
  my $lock = new File::NFSLock {
    file => $datafile,
    lock_type => LOCK_EX | LOCK_NB,
  };
  print $wr2 !!$lock; # Send boolean success status down pipe
  close($wr2); # Signal to parent that the Non-Blocking lock is done
  close($rd2);
  if ($lock) {
    sysopen(my $fh, $datafile, O_RDWR);
    # now put a magic word into the file
    print $fh "child2\n";
    close $fh;
  }
  exit;
}
ok 1; # Fork successful
close ($wr2);
# Waiting for child2 to finish its lock status
my $child2_lock = <$rd2>;
close ($rd2);
# Report status of the child2_lock.
# This lock should not have been obtained since
# the child1 lock should still have been established.
ok (!$child2_lock);

my ($rd3, $wr3);
ok (pipe($rd3, $wr3)); # Connected pipe for child3
if (!fork) {
  # Child #3 process
  my $lock = new File::NFSLock {
    file => $datafile,
    lock_type => LOCK_EX | LOCK_NB,
  };
  print $wr3 !!$lock; # Send boolean success status down pipe
  close($wr3); # Signal to parent that the Non-Blocking lock is done
  close($wr3);
  if ($lock) {
    sysopen(my $fh, $datafile, O_RDWR);
    # now put a magic word into the file
    print $fh "child3\n";
    close $fh;
  }
  exit;
}
ok 1; # Fork successful
close ($wr3);
# Waiting for child2 to finish its lock status
my $child3_lock = <$rd3>;
close ($rd3);
# Report status of the child3_lock.
# This lock should also fail since the child1
# lock should still have been established.
ok (!$child3_lock);

# Wait until the children have finished.
wait; wait; wait;

# Load up whatever the file says now
sysopen(my $fh2, $datafile, O_RDONLY);
$_ = <$fh2>;
close $fh2;

# It should be child1 if it was really nonblocking
# since it got the lock first.
ok /child1/;

# Wipe the temporary file
unlink $datafile;
