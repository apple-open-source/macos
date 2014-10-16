#============================================================= -*-perl-*-
#
# t/ref.t
#
# Template script testing variable references.
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
use lib qw( ../lib );
use Template::Constants qw( :status );
use Template;
use Template::Test;
$^W = 1;

#$Template::Test::DEBUG = 0;
#$Template::Context::DEBUG = 0;
#$Template::Parser::DEBUG = 1;
#$Template::Directive::PRETTY= 1;

local $" = ', ';
my $replace = { 
    a => sub { return "a sub [@_]" },
    j => { k => 3, l => 5, m => { n => sub { "nsub [@_]" } } },
    z => sub { my $sub = shift; return "z called " . &$sub(10, 20, 30) },
};

test_expect(\*DATA, undef, $replace);

__DATA__
-- test --
a: [% a %]
a(5): [% a(5) %]
a(5,10): [% a(5,10) %]
-- expect --
a: a sub []
a(5): a sub [5]
a(5,10): a sub [5, 10]

-- test --
[% b = \a -%]
b: [% b %]
b(5): [% b(5) %]
b(5,10): [% b(5,10) %]
-- expect --
b: a sub []
b(5): a sub [5]
b(5,10): a sub [5, 10]

-- test --
[% c = \a(10,20) -%]
c: [% c %]
c(30): [% c(30) %]
c(30,40): [% c(30,40) %]
-- expect --
c: a sub [10, 20]
c(30): a sub [10, 20, 30]
c(30,40): a sub [10, 20, 30, 40]

-- test --
[% z(\a) %]
-- expect --
z called a sub [10, 20, 30]

-- test --
[% f = \j.k -%]
f: [% f %]
-- expect --
f: 3

-- test --
[% f = \j.m.n -%]
f: [% f %]
f(11): [% f(11) %]
-- expect --
f: nsub []
f(11): nsub [11]


