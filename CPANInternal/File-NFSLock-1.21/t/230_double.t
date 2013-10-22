# Exclusive Double Lock Test
#
# This tests to make sure the same process can aquire
# an exclusive lock multiple times for the same file.

use strict;
use warnings;

use Test::More tests => 5;
use File::NFSLock;
use Fcntl qw(O_CREAT O_RDWR O_RDONLY O_TRUNC O_APPEND LOCK_EX LOCK_SH LOCK_NB);

$| = 1;

my $datafile = "testfile.dat";

# Wipe lock file in case it exists
unlink ("$datafile$File::NFSLock::LOCK_EXTENSION");

# Create a blank file
sysopen ( my $fh, $datafile, O_CREAT | O_RDWR | O_TRUNC );
close ($fh);
ok (-e $datafile && !-s _);


my $lock1 = new File::NFSLock {
  file => $datafile,
  lock_type => LOCK_EX,
  blocking_timeout => 10,
};

ok ($lock1);

sysopen(my $fh2, $datafile, O_RDWR | O_APPEND);
print $fh2 "lock1\n";
close $fh2;

my $lock2 = new File::NFSLock {
  file => $datafile,
  lock_type => LOCK_EX,
  blocking_timeout => 10,
};

ok ($lock2);

sysopen(my $fh3, $datafile, O_RDWR | O_APPEND);
print $fh3 "lock2\n";
close $fh3;

# Load up whatever the file says now
sysopen(my $fh4, $datafile, O_RDONLY);
$_ = <$fh4>;
ok /lock1/;
$_ = <$fh4>;
ok /lock2/;
close $fh4;

# Wipe the temporary file
unlink $datafile;
