#============================================================= -*-perl-*-
#
# t/tags.t
#
# Template script testing TAGS parse-time directive to switch the
# tokens that mark start and end of directive tags.
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
use lib qw( ./lib ../lib ./blib/lib ./blib/arch );
use Template::Test;
$^W = 1;

$Template::Test::DEBUG = 0;

my $params = {
    'a'  => 'alpha',
    'b'  => 'bravo',
    'c'  => 'charlie',
    'd'  => 'delta',
    'e'  => 'echo',
    tags  => 'my tags',
    flags => 'my flags',
};

my $tt = [
    basic => Template->new(INTERPOLATE => 1),
    htags => Template->new(TAG_STYLE => 'html'),
    stags => Template->new(START_TAG => '\[\*',  END_TAG => '\*\]'),
];

test_expect(\*DATA, $tt, $params);

__DATA__
[%a%] [% a %] [% a %]
-- expect --
alpha alpha alpha

-- test --
Redefining tags
[% TAGS (+ +) %]
[% a %]
[% b %]
(+ c +)
-- expect --
Redefining tags

[% a %]
[% b %]
charlie

-- test --
[% a %]
[% TAGS (+ +) %]
[% a %]
%% b %%
(+ c +)
(+ TAGS <* *> +)
(+ d +)
<* e *>
-- expect --
alpha

[% a %]
%% b %%
charlie

(+ d +)
echo

-- test --
[% TAGS default -%]
[% a %]
%% b %%
(+ c +)
-- expect --
alpha
%% b %%
(+ c +)

-- test --
# same as 'default'
[% TAGS template -%]
[% a %]
%% b %%
(+ c +)
-- expect --
alpha
%% b %%
(+ c +)

-- test --
[% TAGS metatext -%]
[% a %]
%% b %%
<* c *>
-- expect --
[% a %]
bravo
<* c *>

-- test --
[% TAGS template1 -%]
[% a %]
%% b %%
(+ c +)
-- expect --
alpha
bravo
(+ c +)

-- test --
[% TAGS html -%]
[% a %]
%% b %%
<!-- c -->
-- expect --
[% a %]
%% b %%
charlie

-- test --
[% TAGS asp -%]
[% a %]
%% b %%
<!-- c -->
<% d %>
<? e ?>
-- expect --
[% a %]
%% b %%
<!-- c -->
delta
<? e ?>

-- test --
[% TAGS php -%]
[% a %]
%% b %%
<!-- c -->
<% d %>
<? e ?>
-- expect --
[% a %]
%% b %%
<!-- c -->
<% d %>
echo

#------------------------------------------------------------------------
# test processor with pre-defined TAG_STYLE
#------------------------------------------------------------------------
-- test --
-- use htags --
[% TAGS ignored -%]
[% a %]
<!-- c -->
more stuff
-- expect --
[% TAGS ignored -%]
[% a %]
charlie
more stuff

#------------------------------------------------------------------------
# test processor with pre-defined START_TAG and END_TAG
#------------------------------------------------------------------------
-- test --
-- use stags --
[% TAGS ignored -%]
<!-- also totally ignored and treated as text -->
[* a *]
blah [* b *] blah
-- expect --
[% TAGS ignored -%]
<!-- also totally ignored and treated as text -->
alpha
blah bravo blah


#------------------------------------------------------------------------
# XML style tags
#------------------------------------------------------------------------

-- test --
-- use basic --
[% TAGS <tt: > -%]
<tt:a=10->
a: <tt:a>
<tt:FOR a = [ 1, 3, 5, 7 ]->
<tt:a>
<tt:END->
-- expect --
a: 10
1
3
5
7

-- test --
[% TAGS star -%]
[* a = 10 -*]
a is [* a *]
-- expect --
a is 10

-- test --
[% tags; flags %]
[* a = 10 -*]
a is [* a *]
-- expect --
my tagsmy flags
[* a = 10 -*]
a is [* a *]

-- test --
flags: [% flags | html %]
tags: [% tags | html %]
-- expect --
flags: my flags
tags: my tags

