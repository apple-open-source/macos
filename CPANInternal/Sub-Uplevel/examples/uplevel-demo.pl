use strict;
use warnings;

use Sub::Uplevel;

# subroutine A calls subroutine B with uplevel(), so when
# subroutine B queries caller(), it gets main as the caller (just
# like subroutine A) instead of getting subroutine A

sub sub_a {
    print "Entering Subroutine A\n";
    print "caller() says: ", join( ", ", (caller())[0 .. 2] ), "\n";
    print "Calling B with uplevel\n";
    uplevel 1, \&sub_b;
}

sub sub_b {
    print "Entering Subroutine B\n";
    print "caller() says: ", join( ", ", (caller())[0 .. 2] ), "\n";
}

sub_a();

