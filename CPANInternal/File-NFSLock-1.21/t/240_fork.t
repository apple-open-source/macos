# Fork Test
#
# This tests the capabilities of fork after lock to
# allow a parent to delegate the lock to its child.

use strict;
use warnings;

use Test::More tests => 5;
use File::NFSLock;
use Fcntl qw(O_CREAT O_RDWR O_RDONLY O_TRUNC O_APPEND LOCK_EX LOCK_SH LOCK_NB);

$| = 1; # Buffer must be autoflushed because of fork() below.

my $datafile = "testfile.dat";

# Wipe lock file in case it exists
unlink ("$datafile$File::NFSLock::LOCK_EXTENSION");

# Create a blank file
sysopen ( my $fh, $datafile, O_CREAT | O_RDWR | O_TRUNC );
close ($fh);
ok (-e $datafile && !-s _);

if (1) {
  # Forced dummy scope
  my $lock1 = new File::NFSLock {
    file => $datafile,
    lock_type => LOCK_EX,
  };

  ok ($lock1);

  my $pid = fork;
  if (!defined $pid) {
    die "fork failed!";
  } elsif (!$pid) {
    # Child process

    # Test possible race condition
    # by making parent reach newpid()
    # and attempt relock before child
    # even calls newpid() the first time.
    sleep 2;
    $lock1->newpid;

    # Act busy for a while
    sleep 5;

    # Now release lock
    exit;
  } else {
    # Fork worked
    ok 1;
    # Avoid releasing lock
    # because child should do it.
    $lock1->newpid;
  }
}
# Lock is out of scope, but
# should still be acquired.

#sysopen(FH, $datafile, O_RDWR | O_APPEND);
#print FH "lock1\n";
#close FH;

# Try to get a non-blocking lock.
# Yes, it is the same process,
# but it should have been delegated
# to the child process.
# This lock should fail.
my $lock2 = new File::NFSLock {
  file => $datafile,
  lock_type => LOCK_EX|LOCK_NB,
};

ok (!$lock2);

# Wait for child to finish
ok(wait);

# Wipe the temporary file
unlink $datafile;
