#============================================================= -*-perl-*-
#
# t/assert.t
#
# Test the assert plugin which throws error if undefined values are
# returned.
#
# Written by Andy Wardley <abw@wardley.org>
#
# Copyright (C) 1996-2008 Andy Wardley.  All Rights Reserved.
#
# This is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
#========================================================================

use strict;
use warnings;
use lib qw( ./lib ../lib ../blib/lib ../blib/arch );
use Template::Test;

#------------------------------------------------------------------------
# definition of test object class
#------------------------------------------------------------------------

package Template::Test::Object;

sub new {
    bless {}, shift;
}

sub nil {
    return undef;
}


#-----------------------------------------------------------------------
# main
#-----------------------------------------------------------------------

package main;

my $vars = { 
    object => Template::Test::Object->new,
    hash   => { foo => 10, bar => undef },
    list   => [ undef ],
    subref => sub { return undef },
    nothing => undef,
};

test_expect(\*DATA, undef, $vars);



#------------------------------------------------------------------------
# test input
#------------------------------------------------------------------------

__DATA__
-- test -- 
([% object.nil %])
-- expect --
()

-- test -- 
[% USE assert;
   TRY; object.assert.nil; CATCH; error; END; "\n";
   TRY; object.assert.zip; CATCH; error; END;
%]
-- expect --
assert error - undefined value for nil
assert error - undefined value for zip

-- test -- 
[% USE assert;
   TRY; hash.assert.bar; CATCH; error; END; "\n";
   TRY; hash.assert.bam; CATCH; error; END;
%]
-- expect --
assert error - undefined value for bar
assert error - undefined value for bam

-- test -- 
[% USE assert;
   TRY; list.assert.0;     CATCH; error; END; "\n";
   TRY; list.assert.first; CATCH; error; END;
%]
-- expect --
assert error - undefined value for 0
assert error - undefined value for first

-- test -- 
[% USE assert;
   TRY; list.assert.0;     CATCH; error; END; "\n";
   TRY; list.assert.first; CATCH; error; END;
%]
-- expect --
assert error - undefined value for 0
assert error - undefined value for first

-- test -- 
[% USE assert;
   TRY; assert.nothing; CATCH; error; END;
%]
-- expect --
assert error - undefined value for nothing

-- test -- 
[% USE assert;
   TRY; assert.subref; CATCH; error; END;
%]
-- expect --
assert error - undefined value for subref


