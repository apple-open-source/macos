#============================================================= -*-perl-*-
#
# t/case.t
#
# Test the CASE sensitivity option.
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

ok(1);

my $ttdef = Template->new({
    POST_CHOMP => 1,
});

my $ttanycase = Template->new({ 
    ANYCASE => 1, 
    POST_CHOMP => 1,
});

my $tts = [ default => $ttdef, anycase => $ttanycase ];

test_expect(\*DATA, $tts, callsign());

__DATA__
-- test --
[% include = a %]
[% for = b %]
i([% include %])
f([% for %])
-- expect --
i(alpha)
f(bravo)

-- test --
[% IF a AND b %]
good
[% ELSE %]
bad
[% END %]
-- expect --
good

-- test --
# 'and', 'or' and 'not' can ALWAYS be expressed in lower case, regardless
# of CASE sensitivity option.
[% IF a and b %]
good
[% ELSE %]
bad
[% END %]
-- expect --
good

-- test --
[% include = a %]
[% include %]
-- expect --
alpha

-- test --
-- use anycase --
[% include foo bar='baz' %]
[% BLOCK foo %]this is foo, bar = [% bar %][% END %]
-- expect --
this is foo, bar = baz

-- test --
[% 10 div 3 %] [% 10 DIV 3 +%]
[% 10 mod 3 %] [% 10 MOD 3 %]
-- expect --
3 3
1 1
