use strict;
use warnings;

use Test::More tests => 3;
use File::NFSLock;
use Fcntl;

# Make sure File::NFSLock has the correct
# constants according to Fcntl
is (&File::NFSLock::LOCK_SH(),&Fcntl::LOCK_SH());
is (&File::NFSLock::LOCK_EX(),&Fcntl::LOCK_EX());
is (&File::NFSLock::LOCK_NB(),&Fcntl::LOCK_NB());
