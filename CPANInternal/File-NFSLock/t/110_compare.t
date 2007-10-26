use Test;
use File::NFSLock;
use Fcntl;

plan tests => 4;

# Everything loaded fine
ok (1);

# Make sure File::NFSLock has the correct
# constants according to Fcntl
ok (&File::NFSLock::LOCK_SH(),&Fcntl::LOCK_SH());
ok (&File::NFSLock::LOCK_EX(),&Fcntl::LOCK_EX());
ok (&File::NFSLock::LOCK_NB(),&Fcntl::LOCK_NB());
