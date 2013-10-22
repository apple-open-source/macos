#============================================================= -*-perl-*-
#
# t/stash-xs.t
#
# Template script testing (some elements of) the XS version of
# Template::Stash
#
# Written by Andy Wardley <abw@wardley.org>
#
# Copyright (C) 1996-2009 Andy Wardley.  All Rights Reserved.
#
# This is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
#========================================================================

use strict;
use warnings;
use lib qw( ./lib ../lib ../blib/lib ../blib/arch ./blib/lib ./blib/arch );
use Template::Constants qw( :status );
use Template;
use Template::Test;

eval {
    require Template::Stash::XS;
};
if ($@) {
    warn $@;
    skip_all('cannot load Template::Stash::XS');
}


#------------------------------------------------------------------------
# define some simple objects for testing
#------------------------------------------------------------------------

package Buggy;
sub new { bless {}, shift }
sub croak { my $self = shift; die @_ }

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

sub now_is_the_time_to_test_a_very_long_method_to_see_what_happens {
    my $self = shift;
    return $self->this_method_does_not_exist();
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
    bop     => sub { return ( bless ({ name => 'an object' }, 'AnObject') ) }, 
    listobj => bless([10, 20, 30], 'ListObject'),
    hashobj => bless({ planet => 'World' }, 'HashObject'),
    cmp_ol  => CmpOverloadObject->new(),
    clean   => sub {
        my $error = shift;
        $error =~ s/(\s*\(.*?\))?\s+at.*$//;
        return $error;
    },
    correct => sub { die @_ },
    buggy   => Buggy->new(),
    str_eval_die => sub {
        # This is to test bug RT#47929
        eval "use No::Such::Module::Exists";
        return "str_eval_die returned";
    },
};

my $stash = Template::Stash::XS->new($data);

match( $stash->get('foo'), 10 );
#match( $stash->get(['foo']), 10 );   # fails
match( $stash->get([ 'bar', 0, 'baz', 0 ]), 20 );
match( $stash->get('bar.baz'), 20 );
match( $stash->get('bar(10).baz'), 20 );
match( $stash->get('baz.boz'), 30 );
match( $stash->get('baz.boz'), 40 );
match( $stash->get('baz.biz'), '<undef>' );
match( $stash->get('baz(50).biz'), '<undef>' );   # args are ignored
#match( $stash->get('str_eval_die'), '' );

$stash->set( 'bar.buz' => 100 );
match( $stash->get('bar.buz'), 100 );

# test the dotop() method
match( $stash->dotop({ foo => 10 }, 'foo'), 10 );

my $stash_dbg = Template::Stash::XS->new({ %$data, _DEBUG => 1 });

my $ttlist = [
    'default' => Template->new( STASH => $stash ),
    'warn'    => Template->new( STASH => $stash_dbg ),
];

test_expect(\*DATA, $ttlist, $data);

__DATA__
-- test --
-- name scalar list method --
[% foo = 'bar'; foo.join %]
-- expect --
bar

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
-- name slice of lemon --
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
-- name slice of toast --
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
[% i = 0 %]
[%- a = [ 0, 1, 2 ] -%]
[%- WHILE i < 3 -%]
[%- i %][% a.$i -%]
[%- i = i + 1 -%]
[%- END %]
-- expect --
001122

-- test --
[%- a = [ "alpha", "beta", "gamma", "delta" ] -%]
[%- b = "foo" -%]
[%- a.$b -%]
-- expect --


-- test --
[%- a = [ "alpha", "beta", "gamma", "delta" ] -%]
[%- b = "2" -%]
[%- a.$b -%]
-- expect --
gamma

-- test --
[% obj.name %]
-- expect --
an object

-- test --
[% obj.name.list.first %]
-- expect --
an object

-- test --
-- name bop --
[% bop.first.name %]
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
=[% size %]=
-- expect --
==

-- test --
[% USE Dumper;
   TRY;
     correct(["hello", "there"]);
   CATCH;
     error.info.join(', ');
   END;
%]
==
[% TRY;
     buggy.croak(["hello", "there"]);
   CATCH;
     error.info.join(', ');
   END;
%]
-- expect --
hello, there
==
hello, there


-- test --
[% hash = { }
   list = [ hash ]
   list.last.message = 'Hello World';
   "message: $list.last.message\n"
-%]

-- expect --
message: Hello World

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

-- test --
[% TRY; 
    hashobj.now_is_the_time_to_test_a_very_long_method_to_see_what_happens;
   CATCH; 
     "ERROR: "; clean(error); 
   END 
%]
-- expect --
ERROR: undef error - Can't locate object method "this_method_does_not_exist" via package "HashObject"

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


# Exercise the object with the funky overloaded comparison

-- test --
[% cmp_ol.hello %]
-- expect --
Hello

-- test --
Before
[%  TRY;
        str_eval_die;
    CATCH;
        "caught error: $error";
    END;
%]
After
-- expect --
Before
str_eval_die returned
After

-- test --
[%  str_eval_die %]
-- expect --
str_eval_die returned

