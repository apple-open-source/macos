#============================================================= -*-perl-*-
#
# t/parser.t
#
# Test the Template::Parser module.
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
use lib qw( . ../lib );
use Template::Test;
use Template::Config;
use Template::Parser;
$^W = 1;

#$Template::Test::DEBUG = 0;
#$Template::Test::PRESERVE = 1;
#$Template::Stash::DEBUG = 1;
#$Template::Parser::DEBUG = 1;
#$Template::Directive::PRETTY = 1;

my $p2 = Template::Parser->new({
    START_TAG => '\[\*',
    END_TAG   => '\*\]',
    ANYCASE   => 1,
    PRE_CHOMP => 1,
    V1DOLLAR  => 1,
});

# test new/old styles
my $s1 = $p2->new_style( { TAG_STYLE => 'metatext', PRE_CHOMP => 0, POST_CHOMP => 1 } )
    || die $p2->error();
ok( $s1 );
match( $s1->{ START_TAG  }, '%%' );
match( $s1->{ PRE_CHOMP  }, '0' );
match( $s1->{ POST_CHOMP }, '1' );

#print STDERR "style: { ", join(', ', map { "$_ => $s1->{ $_ }" } keys %$s1), " }\n";

my $s2 = $p2->old_style()
    || die $p2->error();
ok( $s2 );
match( $s2->{ START_TAG  }, '\[\*' );
match( $s2->{ PRE_CHOMP  }, '1' );
match( $s2->{ POST_CHOMP }, '0' );

#print STDERR "style: { ", join(', ', map { "$_ => $s2->{ $_ }" } keys %$s2), " }\n";

my $p3 = Template::Config->parser({
    TAG_STYLE  => 'html',
    POST_CHOMP => 1,
    ANYCASE    => 1,
    INTERPOLATE => 1,
});

my $p4 = Template::Config->parser({
    ANYCASE => 0,
});

my $tt = [
    tt1 => Template->new(ANYCASE => 1),
    tt2 => Template->new(PARSER => $p2),
    tt3 => Template->new(PARSER => $p3),
    tt4 => Template->new(PARSER => $p4),
];

my $replace = &callsign;
$replace->{ alist  } = [ 'foo', 0, 'bar', 0 ];
$replace->{ wintxt } = "foo\r\n\r\nbar\r\n\r\nbaz";
$replace->{ data   } = { first => 11, last => 42 };

test_expect(\*DATA, $tt, $replace);

__DATA__
#------------------------------------------------------------------------
# tt1
#------------------------------------------------------------------------
-- test --
start $a
[% BLOCK a %]
this is a
[% END %]
=[% INCLUDE a %]=
=[% include a %]=
end
-- expect --
start $a

=
this is a
=
=
this is a
=
end

-- test --
[% data.first; ' to '; data.last %]
-- expect --
11 to 42


#------------------------------------------------------------------------
# tt2
#------------------------------------------------------------------------
-- test --
-- use tt2 --
begin
[% this will be ignored %]
[* a *]
end
-- expect --
begin
[% this will be ignored %]alpha
end

-- test --
$b does nothing: 
[* c = 'b'; 'hello' *]
stuff: 
[* $c *]
-- expect --
$b does nothing: hello
stuff: b

#------------------------------------------------------------------------
# tt3
#------------------------------------------------------------------------
-- test --
-- use tt3 --
begin
[% this will be ignored %]
<!-- a -->
end

-- expect --
begin
[% this will be ignored %]
alphaend

-- test --
$b does something: 
<!-- c = 'b'; 'hello' -->
stuff: 
<!-- $c -->
end
-- expect --
bravo does something: 
hellostuff: 
bravoend


#------------------------------------------------------------------------
# tt4
#------------------------------------------------------------------------
-- test --
-- use tt4 --
start $a[% 'include' = 'hello world' %]
[% BLOCK a -%]
this is a
[%- END %]
=[% INCLUDE a %]=
=[% include %]=
end
-- expect --
start $a

=this is a=
=hello world=
end


#------------------------------------------------------------------------
-- test --
[% sql = "
     SELECT *
     FROM table"
-%]
SQL: [% sql %]
-- expect --
SQL: 
     SELECT *
     FROM table

-- test --
[% a = "\a\b\c\ndef" -%]
a: [% a %]
-- expect --
a: abc
def

-- test --
[% a = "\f\o\o"
   b = "a is '$a'"
   c = "b is \$100"
-%]
a: [% a %]  b: [% b %]  c: [% c %]

-- expect --
a: foo  b: a is 'foo'  c: b is $100

-- test --
[% tag = {
      a => "[\%"
      z => "%\]"
   }
   quoted = "[\% INSERT foo %\]"
-%]
A directive looks like: [% tag.a %] INCLUDE foo [% tag.z %]
The quoted value is [% quoted %]

-- expect --
A directive looks like: [% INCLUDE foo %]
The quoted value is [% INSERT foo %]

-- test --
=[% wintxt | replace("(\r\n){2,}", "\n<break>\n") %]

-- expect --
=foo
<break>
bar
<break>
baz

-- test --
[% nl  = "\n"
   tab = "\t"
-%]
blah blah[% nl %][% tab %]x[% nl; tab %]y[% nl %]end
-- expect --
blah blah
	x
	y
end


#------------------------------------------------------------------------
# STOP RIGHT HERE!
#------------------------------------------------------------------------

-- stop --

-- test --
alist: [% $alist %]
-- expect --
alist: ??

-- test --
[% foo.bar.baz %]
