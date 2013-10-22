#============================================================= -*-perl-*-
#
# t/capture.t
#
# Test that the output from a directive block can be assigned to a 
# variable.
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

my $config = {
    POST_CHOMP => 1,
};

my $replace = {
    a => 'alpha',
    b => 'bravo',
};

test_expect(\*DATA, $config, $replace);

__DATA__

-- test --
[% BLOCK foo %]
This is block foo, a is [% a %]
[% END %]
[% b = INCLUDE foo %]
[% c = INCLUDE foo a = 'ammended' %]
b: <[% b %]>
c: <[% c %]>
-- expect --
b: <This is block foo, a is alpha>
c: <This is block foo, a is ammended>

-- test --
[% d = BLOCK %]
This is the block, a is [% a %]
[% END %]
[% a = 'charlie' %]
a: [% a %]   d: [% d %]
-- expect --
a: charlie   d: This is the block, a is alpha

-- test --
[% e = IF a == 'alpha' %]
a is [% a %]
[% ELSE %]
that was unexpected
[% END %]
e: [% e %]
-- expect --
e: a is alpha

-- test --
[% a = FOREACH b = [1 2 3] %]
[% b %],
[%- END %]
a is [% a %]

-- expect --
a is 1,2,3,

-- test --
[% BLOCK userinfo %]
name: [% user +%]
[% END %]
[% out = PROCESS userinfo FOREACH user = [ 'tom', 'dick', 'larry' ] %]
Output:
[% out %]
-- expect --
Output:
name: tom
name: dick
name: larry



