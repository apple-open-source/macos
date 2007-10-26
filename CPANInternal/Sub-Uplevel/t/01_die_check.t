#!/usr/bin/perl -w

# Kirk:   How we deal with death is at least as important as how we deal 
#         with life, wouldn't you say? 
# Saavik: As I indicated, Admiral, that thought had not occurred to me.  
# Kirk:   Well, now you have something new to think about. Carry on. 
 
# XXX DG: Why is this test here?  Seems pointless.  Oh, well.

use lib qw(t/lib);
use Test::More tests => 1;

#line 12
eval { die };
is( $@, "Died at $0 line 12.\n" );

