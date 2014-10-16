#============================================================= -*-perl-*-
#
# t/stash.t
#
# Template script testing (some elements of) the Template::Stash
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
use Template::Constants qw( :status :debug );
use Template;
use Template::Stash;
use Template::Config;
use Template::Test;
$^W = 1;

my $DEBUG = grep(/-d/, @ARGV);
#$Template::Parser::DEBUG     = $DEBUG;
#$Template::Directive::PRETTY = $DEBUG;

#------------------------------------------------------------------------
# define some simple objects for testing
#------------------------------------------------------------------------

package ListObject;
package HashObject;

sub hello {
    my $self = shift;
    return "Hello $self->{ planet }";
}

sub goodbye {
    my $self = shift;
    return $self->no_such_method();
}

#------------------------------------------------------------------------
# Another object for tracking down a bug with DBIx::Class where TT is 
# causing the numification operator to be called.  Matt S Trout suggests
# we've got a truth test somewhere that should be a defined but that 
# doesn't appear to be the case...
# http://rt.cpan.org/Ticket/Display.html?id=23763
#------------------------------------------------------------------------

package Numbersome;

use overload 
    '""' => 'stringify',
    '0+' => 'numify', 
    fallback => 1;

sub new {
    my ($class, $text) = @_;
    bless \$text, $class;
}

sub numify {
    my $self = shift;
    return "FAIL: numified $$self";
}

sub stringify {
    my $self = shift;
    return "PASS: stringified $$self";
}

sub things {
    return [qw( foo bar baz )];
}

package GetNumbersome;

sub new {
    my ($class, $text) = @_;
    bless { }, $class;
}

sub num {
    Numbersome->new("from GetNumbersome");
}

#-----------------------------------------------------------------------
# another object without overloaded comparison.
# http://rt.cpan.org/Ticket/Display.html?id=24044
#-----------------------------------------------------------------------

package CmpOverloadObject;

use overload ('cmp' => 'compare_overload', '<=>', 'compare_overload');

sub new { bless {}, shift };

sub hello {
    return "Hello";
}

sub compare_overload {
    die "Mayhem!";
}

package main;
    

$Template::Config::STASH = 'Template::Stash';

my $count = 20;
my $data = {
    foo => 10,
    bar => {
        baz => 20,
    },
    baz => sub {
        return {
            boz => ($count += 10),
            biz => (shift || '<undef>'),
        };
    },
    obj => bless({
        name => 'an object',
    }, 'AnObject'),
    bop => sub { return ( bless ({ name => 'an object' }, 'AnObject') ) }, 
    hashobj => bless({ planet => 'World' }, 'HashObject'),
    listobj => bless([10, 20, 30], 'ListObject'),
    num     => Numbersome->new("Numbersome"),
    getnum  => GetNumbersome->new,
    cmp_ol  => CmpOverloadObject->new(),
    clean   => sub {
        my $error = shift;
        $error =~ s/(\s*\(.*?\))?\s+at.*$//;
        return $error;
    },
};

my $stash = Template::Stash->new($data);

match( $stash->get('foo'), 10 );
match( $stash->get([ 'bar', 0, 'baz', 0 ]), 20 );
match( $stash->get('bar.baz'), 20 );
match( $stash->get('bar(10).baz'), 20 );
match( $stash->get('baz.boz'), 30 );
match( $stash->get('baz.boz'), 40 );
match( $stash->get('baz.biz'), '<undef>' );
match( $stash->get('baz(50).biz'), '<undef>' );   # args are ignored

$stash->set( 'bar.buz' => 100 );
match( $stash->get('bar.buz'), 100 );

# test the dotop() method
match( $stash->dotop({ foo => 10 }, 'foo'), 10 );


my $ttlist = [
    'default' => Template->new(),
    'warn'    => Template->new(DEBUG => DEBUG_UNDEF, DEBUG_FORMAT => ''),
];

test_expect(\*DATA, $ttlist, $data);

__DATA__
-- test --
a: [% a %]
-- expect --
a: 

-- test --
-- use warn --
[% TRY; a; CATCH; "ERROR: $error"; END %]
-- expect --
ERROR: undef error - a is undefined

-- test --
-- use default --
[% myitem = 'foo' -%]
1: [% myitem %]
2: [% myitem.item %]
3: [% myitem.item.item %]
-- expect --
1: foo
2: foo
3: foo

-- test --
[% myitem = 'foo' -%]
[% "* $item\n" FOREACH item = myitem -%]
[% "+ $item\n" FOREACH item = myitem.list %]
-- expect --
* foo
+ foo

-- test --
[% myitem = 'foo' -%]
[% myitem.hash.value %]
-- expect --
foo

-- test --
[% myitem = 'foo'
   mylist = [ 'one', myitem, 'three' ]
   global.mylist = mylist
-%]
[% mylist.item %]
0: [% mylist.item(0) %]
1: [% mylist.item(1) %]
2: [% mylist.item(2) %]
-- expect --
one
0: one
1: foo
2: three

-- test --
[% "* $item\n" FOREACH item = global.mylist -%]
[% "+ $item\n" FOREACH item = global.mylist.list -%]
-- expect --
* one
* foo
* three
+ one
+ foo
+ three

-- test --
[% global.mylist.push('bar');
   "* $item.key => $item.value\n" FOREACH item = global.mylist.hash -%]
-- expect --
* one => foo
* three => bar

-- test --
[% myhash = { msg => 'Hello World', things => global.mylist, a => 'alpha' };
   global.myhash = myhash 
-%]
* [% myhash.item('msg') %]
-- expect --
* Hello World

-- test --
[% global.myhash.delete('things') -%]
keys: [% global.myhash.keys.sort.join(', ') %]
-- expect --
keys: a, msg

-- test --
[% "* $item\n" 
    FOREACH item IN global.myhash.items.sort -%]
-- expect --
* a
* alpha
* Hello World
* msg

-- test --
[% items = [ 'foo', 'bar', 'baz' ];
   take  = [ 0, 2 ];
   slice = items.$take;
   slice.join(', ');
%]
-- expect --
foo, baz

-- test --
[% items = {
    foo = 'one',
    bar = 'two',
    baz = 'three'
   }
   take  = [ 'foo', 'baz' ];
   slice = items.$take;
   slice.join(', ');
%]
-- expect --
one, three

-- test --
[% items = {
    foo = 'one',
    bar = 'two',
    baz = 'three'
   }
   keys = items.keys.sort;
   items.${keys}.join(', ');
%]
-- expect --
two, three, one


-- test --
[% obj.name %]
-- expect --
an object

-- test --
[% obj.name.list.first %]
-- expect --
an object

-- test --
[% obj.items.first %]
-- expect --
name

-- test --
[% obj.items.1 %]
-- expect --
an object

-- test --
[% bop.first.name %]
-- expect --
an object

-- test --
[% listobj.0 %] / [% listobj.first %]
-- expect --
10 / 10

-- test --
[% listobj.2 %] / [% listobj.last %]
-- expect --
30 / 30

-- test --
[% listobj.join(', ') %]
-- expect --
10, 20, 30

-- test --
=[% size %]=
-- expect --
==

-- test --
[% foo = { "one" = "bar" "" = "empty" } -%]
foo is { [% FOREACH k IN foo.keys.sort %]"[% k %]" = "[% foo.$k %]" [% END %]}
setting foo.one to baz
[% fookey = "one" foo.$fookey = "baz" -%]
foo is { [% FOREACH k IN foo.keys.sort %]"[% k %]" = "[% foo.$k %]" [% END %]}
setting foo."" to quux
[% fookey = "" foo.$fookey = "full" -%]
foo is { [% FOREACH k IN foo.keys.sort %]"[% k %]" = "[% foo.$k %]" [% END %]}
--expect --
foo is { "" = "empty" "one" = "bar" }
setting foo.one to baz
foo is { "" = "empty" "one" = "baz" }
setting foo."" to quux
foo is { "" = "full" "one" = "baz" }


# test Dave Howorth's patch (v2.15) which makes the stash more strict
# about what it considers to be a missing method error

-- test --
[% hashobj.hello %]
-- expect --
Hello World

-- test --
[% TRY; hashobj.goodbye; CATCH; "ERROR: "; clean(error); END %]
-- expect --
ERROR: undef error - Can't locate object method "no_such_method" via package "HashObject"


#-----------------------------------------------------------------------
# try and pin down the numification bug
#-----------------------------------------------------------------------

-- test --
[% FOREACH item IN num.things -%]
* [% item %]
[% END -%]
-- expect --
* foo
* bar
* baz

-- test --
[% num %]
-- expect --
PASS: stringified Numbersome

-- test --
[% getnum.num %]
-- expect --
PASS: stringified from GetNumbersome


# Exercise the object with the funky overloaded comparison

-- test --
[% cmp_ol.hello %]
-- expect --
Hello
