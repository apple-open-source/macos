#!./perl

# $RCSfile: if.t,v $$Revision: 1.1.1.2 $$Date: 2000/03/31 05:12:25 $

print "1..2\n";

# first test to see if we can run the tests.

$x = 'test';
if ($x eq $x) { print "ok 1\n"; } else { print "not ok 1\n";}
if ($x ne $x) { print "not ok 2\n"; } else { print "ok 2\n";}
