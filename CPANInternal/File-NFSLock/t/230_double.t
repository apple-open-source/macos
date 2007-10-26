# Exclusive Double Lock Test
#
# This tests to make sure the same process can aquire
# an exclusive lock multiple times for the same file.

use strict;
use Test;
use File::NFSLock;
use Fcntl qw(O_CREAT O_RDWR O_RDONLY O_TRUNC O_APPEND LOCK_EX LOCK_SH LOCK_NB);

$| = 1;
plan tests => 5;

my $datafile = "testfile.dat";

# Wipe lock file in case it exists
unlink ("$datafile$File::NFSLock::LOCK_EXTENSION");

# Create a blank file
sysopen ( FH, $datafile, O_CREAT | O_RDWR | O_TRUNC );
close (FH);
ok (-e $datafile && !-s _);


my $lock1 = new File::NFSLock {
  file => $datafile,
  lock_type => LOCK_EX,
  blocking_timeout => 10,
};

ok ($lock1);

sysopen(FH, $datafile, O_RDWR | O_APPEND);
print FH "lock1\n";
close FH;

my $lock2 = new File::NFSLock {
  file => $datafile,
  lock_type => LOCK_EX,
  blocking_timeout => 10,
};

ok ($lock2);

sysopen(FH, $datafile, O_RDWR | O_APPEND);
print FH "lock2\n";
close FH;

# Load up whatever the file says now
sysopen(FH, $datafile, O_RDONLY);
$_ = <FH>;
ok /lock1/;
$_ = <FH>;
ok /lock2/;
close FH;

# Wipe the temporary file
unlink $datafile;
