#!./perl

# $RCSfile: cpp.t,v $$Revision: 1.1.1.2 $$Date: 2000/03/31 05:12:27 $

BEGIN {
    chdir 't' if -d 't';
    unshift @INC, '../lib';
}

use Config;
if ( $^O eq 'MSWin32' or
     ($Config{'cppstdin'} =~ /\bcppstdin\b/) and
     ( ! -x $Config{'binexp'} . "/cppstdin") ) {
    print "1..0 # Skip: \$Config{cppstdin} unavailable\n";
    exit; 		# Cannot test till after install, alas.
}

system "./perl -P comp/cpp.aux"
