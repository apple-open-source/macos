# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl Unix-Getrusage.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use Test::More tests => 1;
BEGIN { use_ok('Unix::Getrusage') };

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

# $Id: Unix-Getrusage.t 2 2006-02-03 22:35:54Z taffy $
