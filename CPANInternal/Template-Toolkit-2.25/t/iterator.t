#============================================================= -*-perl-*-
#
# t/iterator.t
#
# Template script testing Template::Iterator and 
# Template::Plugin::Iterator.
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
use Template::Iterator;
$^W = 1;

#$Template::Parser::DEBUG = 0;
#$Template::Test::DEBUG = 0;

my $data = [ qw( foo bar baz qux wiz woz waz ) ];
my $vars = {
    data => $data,
#    iterator => Template::Iterator->new($data),
};

my $i1 = Template::Iterator->new($data);
ok( $i1->get_first() eq 'foo' );
ok( $i1->get_next()  eq 'bar' );
ok( $i1->get_next()  eq 'baz' );

my $rest = $i1->get_all();
ok( scalar @$rest == 4 );
ok( $rest->[0] eq 'qux' );
ok( $rest->[3] eq 'waz' );

my ($val, $err) = $i1->get_next();
ok( ! $val );
ok( $err == Template::Constants::STATUS_DONE );

($val, $err) = $i1->get_all();
ok( ! $val );
ok( $err == Template::Constants::STATUS_DONE );

($val, $err) = $i1->get_first();
ok( $i1->get_first() eq 'foo' );
ok( $i1->get_next()  eq 'bar' );
$rest = $i1->get_all();
ok( scalar @$rest == 5 );

# get_all with a few values in the iterator
my $i2 = Template::Iterator->new($data);
($rest, $err) = $i2->get_all();
is( scalar @$rest, 7 );
ok( ! $err);
($val, $err) = $i2->get_all();
ok( ! $val);
is( $err, Template::Constants::STATUS_DONE );

# get_all with a single value.
my $i3 = Template::Iterator->new(['foo']);
($rest, $err) = $i3->get_all();
is( scalar @$rest, 1 );
is( pop @$rest, 'foo' );
ok( ! $err);
($val, $err) = $i3->get_all();
ok( ! $val);
is( $err, Template::Constants::STATUS_DONE );

# get_all with an empty array
my $i4 = Template::Iterator->new([]);
($val, $err) = $i4->get_all();
ok( ! $val);
is( $err, Template::Constants::STATUS_DONE );

test_expect(\*DATA, { POST_CHOMP => 1 }, $vars);

__DATA__

-- test --
[% items = [ 'foo' 'bar' 'baz' 'qux' ] %]
[% FOREACH i = items %]
   * [% i +%]
[% END %]
-- expect --
   * foo
   * bar
   * baz
   * qux

-- test --
[% items = [ 'foo' 'bar' 'baz' 'qux' ] %]
[% FOREACH i = items %]
   #[% loop.index %]/[% loop.max %] [% i +%]
[% END %]
-- expect --
   #0/3 foo
   #1/3 bar
   #2/3 baz
   #3/3 qux

-- test --
[% items = [ 'foo' 'bar' 'baz' 'qux' ] %]
[% FOREACH i = items %]
   #[% loop.count %]/[% loop.size %] [% i +%]
[% END %]
-- expect --
   #1/4 foo
   #2/4 bar
   #3/4 baz
   #4/4 qux

-- test --
# test that 'number' is supported as an alias to 'count', for backwards
# compatability
[% items = [ 'foo' 'bar' 'baz' 'qux' ] %]
[% FOREACH i = items %]
   #[% loop.number %]/[% loop.size %] [% i +%]
[% END %]
-- expect --
   #1/4 foo
   #2/4 bar
   #3/4 baz
   #4/4 qux

-- test --
[% USE iterator(data) %]
[% FOREACH i = iterator %]
[% IF iterator.first %]
List of items:
[% END %]
   * [% i +%]
[% IF iterator.last %]
End of list
[% END %]
[% END %]
-- expect --
List of items:
   * foo
   * bar
   * baz
   * qux
   * wiz
   * woz
   * waz
End of list


-- test --
[% FOREACH i = [ 'foo' 'bar' 'baz' 'qux' ] %]
[% "$loop.prev<-" IF loop.prev -%][[% i -%]][% "->$loop.next" IF loop.next +%]
[% END %]
-- expect --
[foo]->bar
foo<-[bar]->baz
bar<-[baz]->qux
baz<-[qux]

-- test --
-- name test even/odd/parity --
[% FOREACH item IN [1..10] -%]
* [% loop.count %] [% loop.odd %] [% loop.even %] [% loop.parity +%]
[% END -%]
-- expect --
* 1 1 0 odd
* 2 0 1 even
* 3 1 0 odd
* 4 0 1 even
* 5 1 0 odd
* 6 0 1 even
* 7 1 0 odd
* 8 0 1 even
* 9 1 0 odd
* 10 0 1 even
