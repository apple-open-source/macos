#============================================================= -*-perl-*-
#
# t/text.t
#
# Test general text blocks, ensuring all characters can be used.
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

#------------------------------------------------------------------------
package Stringy;

use overload '""' => \&asString;

sub asString {
    my $self = shift;
    return $$self;
}

sub new {
    my ($class, $val) = @_;
    return bless \$val, $class;
}

#------------------------------------------------------------------------
package main;

my $tt = [
    basic  => Template->new(),
    interp => Template->new(INTERPOLATE => 1),
];

my $vars = callsign();

my $v2 = {
    ref    => sub { my $a = shift; "$a\[" . ref($a) . ']' },
    sfoo   => Stringy->new('foo'),
    sbar   => Stringy->new('bar'),
};

@$vars{ keys %$v2 } = values %$v2;

test_expect(\*DATA, $tt, $vars);

__DATA__
-- test --
This is a text block "hello" 'hello' 1/3 1\4 <html> </html>
$ @ { } @{ } ${ } # ~ ' ! % *foo
$a ${b} $c
-- expect --
This is a text block "hello" 'hello' 1/3 1\4 <html> </html>
$ @ { } @{ } ${ } # ~ ' ! % *foo
$a ${b} $c

-- test --
<table width=50%>&copy;
-- expect --
<table width=50%>&copy;

-- test --
[% foo = 'Hello World' -%]
start
[%
#
# [% foo %]
#
#
-%]
end
-- expect --
start
end

-- test --
pre
[%
# [% PROCESS foo %]
-%]
mid
[% BLOCK foo; "This is foo"; END %]
-- expect --
pre
mid

-- test --
-- use interp --
This is a text block "hello" 'hello' 1/3 1\4 <html> </html>
\$ @ { } @{ } \${ } # ~ ' ! % *foo
$a ${b} $c
-- expect --
This is a text block "hello" 'hello' 1/3 1\4 <html> </html>
$ @ { } @{ } ${ } # ~ ' ! % *foo
alpha bravo charlie

-- test --
<table width=50%>&copy;
-- expect --
<table width=50%>&copy;

-- test --
[% foo = 'Hello World' -%]
start
[%
#
# [% foo %]
#
#
-%]
end
-- expect --
start
end

-- test --
pre
[%
#
# [% PROCESS foo %]
#
-%]
mid
[% BLOCK foo; "This is foo"; END %]
-- expect --
pre
mid

-- test --
[% a = "C'est un test"; a %]
-- expect --
C'est un test

-- test --
[% META title = "C'est un test" -%]
[% component.title -%]
-- expect --
C'est un test

-- test --
[% META title = 'C\'est un autre test' -%]
[% component.title -%]
-- expect --
C'est un autre test

-- test --
[% META title = "C'est un \"test\"" -%]
[% component.title -%]
-- expect --
C'est un "test"

-- test --
[% sfoo %]/[% sbar %]
-- expect --
foo/bar

-- test --
[%  s1 = "$sfoo"
    s2 = "$sbar ";
    s3  = sfoo;
    ref(s1);
    '/';
    ref(s2);
    '/';
    ref(s3);
-%]
-- expect --
foo[]/bar []/foo[Stringy]

