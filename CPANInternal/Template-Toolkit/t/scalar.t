#============================================================= -*-perl-*-
#
# t/scalar.t
#
# Test the Scalar plugin which allows object methods to be called in
# scalar context.
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

package Template::Test::HashObject;

sub new {
    bless {}, shift;
}

sub bar {
    return wantarray
         ? qw( hash object method called in array context )
         :    'hash object method called in scalar context';
}

package Template::Test::ListObject;

sub new {
    bless [], shift;
}

sub bar {
    return wantarray
         ? qw( list object method called in array context )
         :    'list object method called in scalar context';
}


#-----------------------------------------------------------------------
# main
#-----------------------------------------------------------------------

package main;

my $vars = { 
    hashobj => Template::Test::HashObject->new,
    listobj => Template::Test::ListObject->new,
    subref  => sub {
        return wantarray
            ? (qw( subroutine called in array context ), @_)
            :    'subroutine called in scalar context ' . join(' ', @_);
    }
};

test_expect(\*DATA, undef, $vars);



#------------------------------------------------------------------------
# test input
#------------------------------------------------------------------------

__DATA__
-- test -- 
[% hashobj.bar.join %]
-- expect --
hash object method called in array context

-- test --
[% USE scalar -%]
[% hashobj.scalar.bar %]
-- expect --
hash object method called in scalar context

-- test -- 
[% listobj.bar.join %]
-- expect --
list object method called in array context

-- test --
[% USE scalar -%]
[% listobj.scalar.bar %]
-- expect --
list object method called in scalar context

-- test --
[% hash = { a = 10 }; 
   TRY; hash.scalar.a; CATCH; error; END;
%]
-- expect --
scalar error - invalid object method: a

-- test --
[% subref(10, 20).join %]
-- expect --
subroutine called in array context 10 20

-- test --
[% USE scalar; scalar.subref(30, 40) %]
-- expect --
subroutine called in scalar context 30 40
