# Blocking Shared Lock Test
use strict;
use warnings;

use Test::More;
if( $^O eq 'MSWin32' ) {
  plan skip_all => 'Tests fail on Win32 due to forking';
}
else {
  plan tests => 13+3*20;
}
use File::NFSLock;
use Fcntl qw(O_CREAT O_RDWR O_RDONLY O_TRUNC O_APPEND LOCK_EX LOCK_NB LOCK_SH);

# $m simultaneous processes trying to obtain a shared lock
my $m = 20;
my $shared_delay = 5;

$| = 1; # Buffer must be autoflushed because of fork() below.

my $datafile = "testfile.dat";

# Create a blank file
sysopen ( my $fh, $datafile, O_CREAT | O_RDWR | O_TRUNC );
close ($fh);
# test 1
ok (-e $datafile && !-s _);


my ($rd1, $wr1);
ok (pipe($rd1, $wr1)); # Connected pipe for child1
if (!fork) {
  # Child #1 process
  # Obtain exclusive lock to block the shared attempt later
  my $lock = new File::NFSLock {
    file => $datafile,
    lock_type => LOCK_EX,
  };
  print $wr1 !!$lock; # Send boolean success status down pipe
  close($wr1); # Signal to parent that the Blocking lock is done
  close($rd1);
  if ($lock) {
    sleep 2;  # hold the lock for a moment
    sysopen(my $fh, $datafile, O_RDWR | O_TRUNC);
    # And then put a magic word into the file
    print $fh "exclusive\n";
    close $fh;
  }
  exit;
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


my ($rd2, $wr2);
ok (pipe($rd2, $wr2)); # Connected pipe for child2
if (!fork) {
  # This should block until the exclusive lock is done
  my $lock = new File::NFSLock {
    file => $datafile,
    lock_type => LOCK_SH,
  };
  if ($lock) {
    sysopen(my $fh, $datafile, O_RDWR | O_TRUNC);
    # Immediately put the magic word into the file
    print $fh "shared\n";
    truncate ($fh, tell $fh);
    close $fh;
    # Normally shared locks never modify the contents because
    # of the race condition.  (The last one to write wins.)
    # But in this case, the parent will wait until the lock
    # status is reported (close RD2) so it defines execution
    # sequence will be correct.  Hopefully the shared lock
    # will not happen until the exclusive lock has been released.
    # This is also a good test to make sure that other shared
    # locks can still be obtained simultaneously.
  }
  print $wr2 !!$lock; # Send boolean success status down pipe
  close($wr2); # Signal to parent that the Blocking lock is done
  close($rd2);
  # Then hold this shared lock for a moment
  # while other shared locks are attempted
  sleep($shared_delay*2);
  exit; # Release the shared lock
}
# test 6
ok 1; # Fork successful
close ($wr2);
# Waiting for child2 to finish its lock status
my $child2_lock = <$rd2>;
close ($rd2);
# Report status of the child2_lock.
# This should have eventually been successful.
# test 7
ok ($child2_lock);

# If all these processes take longer than $shared_delay seconds,
# then they are probably not running synronously
# and the shared lock is not working correctly.
# But if all the children obatin the lock simultaneously,
# like they're supposed to, then it shouldn't take
# much longer than the maximum delay of any of the
# shared locks (at least 5 seconds set above).
$SIG{ALRM} = sub {
  # test (unknown)
  ok 0;
  die "Shared locks not running simultaneously";
};

# Use pipe to read lock success status from children
# test 8
my ($rd3, $wr3);
ok (pipe($rd3, $wr3));

# Wait a few seconds less than if all locks were
# aquired asyncronously to ensure that they overlap.
alarm($m*$shared_delay-2);

for (my $i = 0; $i < $m ; $i++) {
  if (!fork) {
    # All of these locks should immediately be successful since
    # there already exist a shared lock.
    my $lock = new File::NFSLock {
      file => $datafile,
      lock_type => LOCK_SH,
    };
    # Send boolean success status down pipe
    print $wr3 !!$lock,"\n";
    close($wr3);
    if ($lock) {
      sleep $shared_delay;  # Hold the shared lock for a moment
      # Appending should always be safe across NFS
      sysopen(my $fh, $datafile, O_RDWR | O_APPEND);
      # Put one line to signal the lock was successful.
      print $fh "1\n";
      close $fh;
      $lock->unlock();
    } else {
      warn "Lock [$i] failed!";
    }
    exit;
  }
}

# Parent process never writes to pipe
close($wr3);


# There were $m children attempting the shared locks.
for (my $i = 0; $i < $m ; $i++) {
  # Report status of each lock attempt.
  my $got_shared_lock = <$rd3>;
  # test 9 .. 8+$m
  ok $got_shared_lock;
}

# There should not be anything left in the pipe.
my $extra = <$rd3>;
# test 9 + $m
ok !$extra;
close ($rd3);

# If we made it here, then it must have been faster
# than the timeout.  So reset the timer.
alarm(0);
# test 10 + $m
ok 1;

# There are $m children plus the child1 exclusive locker
# and the child2 obtaining the first shared lock.
for (my $i = 0; $i < $m + 2 ; $i++) {
  # Wait until all the children are finished.
  wait;
  # test 11+$m .. 12+2*$m
  ok 1;
}

# Load up whatever the file says now
sysopen(my $fh2, $datafile, O_RDONLY);

# The first line should say "shared" if child2 really
# waited for child1's exclusive lock to finish.
$_ = <$fh2>;
# test 13 + 2*$m
ok /shared/;

for (my $i = 0; $i < $m ; $i++) {
  $_ = <$fh2>;
  chomp;
  # test 14+2*$m .. 13+3*$m
  is $_, 1;
}
close $fh2;

# Wipe the temporary file
unlink $datafile;
