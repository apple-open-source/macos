#============================================================= -*-perl-*-
#
# t/switch.t
#
# Template script testing SWITCH / CASE blocks
#
# Written by Andy Wardley <abw@kfs.org>
#
# Copyright (C) 1996-2000 Andy Wardley.  All Rights Reserved.
# Copyright (C) 1998-2000 Canon Research Centre Europe Ltd.
#
# This is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
# $Id$
#
#========================================================================

use strict;
use lib qw( ./lib ../lib );
use Template::Test;
$^W = 1;

$Template::Test::DEBUG = 0;
#$Template::Parser::DEBUG = 0;

my $ttcfg = {
#    INCLUDE_PATH => [ qw( t/test/lib test/lib ) ],	
    POST_CHOMP   => 1,
};

test_expect(\*DATA, $ttcfg, &callsign());

__DATA__
#------------------------------------------------------------------------
# test simple case
#------------------------------------------------------------------------
-- test --
before
[% SWITCH a %]
this is ignored
[% END %]
after

-- expect --
before
after

-- test --
before
[% SWITCH a %]
this is ignored
[% CASE x %]
not matched
[% END %]
after

-- expect --
before
after

-- test --
before
[% SWITCH a %]
this is ignored
[% CASE not_defined %]
not matched
[% END %]
after

-- expect --
before
after

-- test --
before
[% SWITCH a %]
this is ignored
[% CASE 'alpha' %]
matched
[% END %]
after

-- expect --
before
matched
after

-- test --
before
[% SWITCH a %]
this is ignored
[% CASE a %]
matched
[% END %]
after

-- expect --
before
matched
after

-- test --
before
[% SWITCH 'alpha' %]
this is ignored
[% CASE a %]
matched
[% END %]
after

-- expect --
before
matched
after

-- test --
before
[% SWITCH a %]
this is ignored
[% CASE b %]
matched
[% END %]
after

-- expect --
before
after

-- test --
before
[% SWITCH a %]
this is ignored
[% CASE a %]
matched
[% CASE b %]
not matched
[% END %]
after

-- expect --
before
matched
after

-- test --
before
[% SWITCH a %]
this is ignored
[% CASE b %]
not matched
[% CASE a %]
matched
[% END %]
after

-- expect --
before
matched
after

#------------------------------------------------------------------------
# test default case
#------------------------------------------------------------------------
-- test --
before
[% SWITCH a %]
this is ignored
[% CASE a %]
matched
[% CASE b %]
not matched
[% CASE %]
default not matched
[% END %]
after

-- expect --
before
matched
after

-- test --
before
[% SWITCH a %]
this is ignored
[% CASE a %]
matched
[% CASE b %]
not matched
[% CASE DEFAULT %]
default not matched
[% END %]
after

-- expect --
before
matched
after

-- test --
before
[% SWITCH a %]
this is ignored
[% CASE z %]
not matched
[% CASE x %]
not matched
[% CASE %]
default matched
[% END %]
after

-- expect --
before
default matched
after


-- test --
before
[% SWITCH a %]
this is ignored
[% CASE z %]
not matched
[% CASE x %]
not matched
[% CASE DEFAULT %]
default matched
[% END %]
after

-- expect --
before
default matched
after

#------------------------------------------------------------------------
# test multiple matches
#------------------------------------------------------------------------

-- test --
before
[% SWITCH a %]
this is ignored
[% CASE [ a b c ] %]
matched
[% CASE d %]
not matched
[% CASE %]
default not matched
[% END %]
after

-- expect --
before
matched
after

-- test --
before
[% SWITCH a %]
this is ignored
[% CASE [ a b c ] %]
matched
[% CASE a %]
not matched, no drop-through
[% CASE DEFAULT %]
default not matched
[% END %]
after

-- expect --
before
matched
after


#-----------------------------------------------------------------------
# regex metacharacter quoting
# http://rt.cpan.org/Ticket/Display.html?id=24183
#-----------------------------------------------------------------------

-- test --
[% foo = 'a(b)'
   bar = 'a(b)';

   SWITCH foo;
     CASE bar;
       'ok';
     CASE;
       'not ok';
   END 
%]
-- expect --
ok
