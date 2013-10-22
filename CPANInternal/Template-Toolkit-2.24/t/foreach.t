#============================================================= -*-perl-*-
#
# t/foreach.t
#
# Template script testing the FOREACH directive.
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
use warnings;
use lib qw( ./lib ../lib );
use Template qw( :status );
use Template::Test;

#$Template::Test::DEBUG = 0;
#$Template::Parser::DEBUG = 1;
#$Template::Directive::PRETTY = 1;


my ($a, $b, $c, $d, $l, $o, $r, $u, $w ) = 
	qw( alpha bravo charlie delta lima oscar romeo uncle whisky );

my $day      = -1;
my @days     = qw( Monday Tuesday Wednesday Thursday Friday Saturday Sunday );
my @months   = qw( jan feb mar apr may jun jul aug sep oct nov dec );
my @people   = ( { 'id' => 'abw', 'name' => 'Andy Wardley' },
                 { 'id' => 'sam', 'name' => 'Simon Matthews' } );
my @seta     = ( $a, $b, $w );
my @setb     = ( $c, $l, $o, $u, $d );


my $params   = {
    'a'      => $a,
    'b'      => $b,
    'c'      => $c,
    'C'      => uc $c,
    'd'      => $d,
    'l'      => $l,
    'o'      => $o,
    'r'      => $r,
    'u'      => $u,
    'w'      => $w,
    'seta'   => \@seta,
    'setb'   => \@setb,
    'users'  => \@people,
    'item'   => 'foo',
    'items'  => [ 'foo', 'bar' ],
    'days'   => \@days,
    'months' => sub { return \@months },
    'format' => \&format,
    'people' => [ 
    	{ id => 'abw', code => 'abw', name => 'Andy Wardley' },
    	{ id => 'aaz', code => 'zaz', name => 'Azbaz Azbaz Zazbazzer' },
    	{ id => 'bcd', code => 'dec', name => 'Binary Coded Decimal' },
    	{ id => 'efg', code => 'zzz', name => 'Extra Fine Grass' },
    ],
    'sections' => {
        one   => 'Section One',
        two   => 'Section Two',
        three => 'Section Three',
        four  => 'Section Four',
    },
    nested => [
               [ qw( a b c ) ],
               [ qw( x y z ) ],
    ],
};

sub format {
    my $format = shift;
    $format = '%s' unless defined $format;
    return sub {
	sprintf($format, shift);
    }
}

my $template = Template->new({ 
    INTERPOLATE => 1, 
    POST_CHOMP  => 1, 
    ANYCASE     => 0
});

my $ttdebug = Template->new({
    DEBUG => 1,
    DEBUG_FORMAT => '',
});

test_expect(\*DATA, [ default => $template, debug => $ttdebug ], $params);

__DATA__
-- test --
[% FOREACH a = [ 1, 2, 3 ] %]
   [% a +%]
[% END %]

[% FOREACH foo.bar %]
   [% a %]
[% END %]
-- expect --
   1
   2
   3

-- test --
Commence countdown...
[% FOREACH count = [ 'five' 'four' 'three' 'two' 'one' ] %]
  [% count +%]
[% END %]
Fire!
-- expect --
Commence countdown...
  five
  four
  three
  two
  one
Fire!

-- test --
[% FOR count = [ 1 2 3 ] %]${count}..[% END %]
-- expect --
1..2..3..

-- test --
people:
[% bloke = r %]
[% people = [ c, bloke, o, 'frank' ] %]
[% FOREACH person = people %]
  [ [% person %] ]
[% END %]
-- expect --
people:
  [ charlie ]
  [ romeo ]
  [ oscar ]
  [ frank ]

-- test --
[% FOREACH name = setb %]
[% name %],
[% END %]
-- expect --
charlie,
lima,
oscar,
uncle,
delta,

-- test --
[% FOREACH name = r %]
[% name %], $name, wherefore art thou, $name?
[% END %]
-- expect --
romeo, romeo, wherefore art thou, romeo?

-- test --
[% user = 'fred' %]
[% FOREACH user = users %]
   $user.name ([% user.id %])
[% END %]
   [% user.name %]
-- expect --
   Andy Wardley (abw)
   Simon Matthews (sam)
   Simon Matthews

-- test --
[% name = 'Joe Random Hacker' id = 'jrh' %]
[% FOREACH users %]
   $name ([% id %])
[% END %]
   $name ($id)
-- expect --
   Andy Wardley (abw)
   Simon Matthews (sam)
   Joe Random Hacker (jrh)

-- test --
[% FOREACH i = [1..4] %]
[% i +%]
[% END %]
-- expect --
1
2
3
4

-- test --
[% first = 4 
   last  = 8
%]
[% FOREACH i = [first..last] %]
[% i +%]
[% END %]
-- expect --
4
5
6
7
8

-- test --
[% list = [ 'one' 'two' 'three' 'four' ] %]
[% list.0 %] [% list.3 %]

[% FOREACH n = [0..3] %]
[% list.${n} %], 
[%- END %]

-- expect --
one four
one, two, three, four, 

-- test --
[% "$i, " FOREACH i = [-2..2] %]

-- expect --
-2, -1, 0, 1, 2, 

-- test --
[% FOREACH i = item -%]
    - [% i %]
[% END %]
-- expect --
    - foo

-- test --
[% FOREACH i = items -%]
    - [% i +%]
[% END %]
-- expect --
    - foo
    - bar

-- test --
[% FOREACH item = [ a b c d ] %]
$item
[% END %]
-- expect --
alpha
bravo
charlie
delta

-- test --
[% items = [ d C a c b ] %]
[% FOREACH item = items.sort %]
$item
[% END %]
-- expect --
alpha
bravo
CHARLIE
charlie
delta

-- test --
[% items = [ d a c b ] %]
[% FOREACH item = items.sort.reverse %]
$item
[% END %]
-- expect --
delta
charlie
bravo
alpha

-- test --
[% userlist = [ b c d a C 'Andy' 'tom' 'dick' 'harry' ] %]
[% FOREACH u = userlist.sort %]
$u
[% END %]
-- expect --
alpha
Andy
bravo
charlie
CHARLIE
delta
dick
harry
tom

-- test --
[% ulist = [ b c d a 'Andy' ] %]
[% USE f = format("[- %-7s -]\n") %]
[% f(item) FOREACH item = ulist.sort %]
-- expect --
[- alpha   -]
[- Andy    -]
[- bravo   -]
[- charlie -]
[- delta   -]

-- test --
[% FOREACH item = [ a b c d ] %]
[% "List of $loop.size items:\n" IF loop.first %]
  #[% loop.number %]/[% loop.size %]: [% item +%]
[% "That's all folks\n" IF loop.last %]
[% END %]
-- expect --
List of 4 items:
  #1/4: alpha
  #2/4: bravo
  #3/4: charlie
  #4/4: delta
That's all folks

-- test --
[% items = [ d b c a ] %]
[% FOREACH item = items.sort %]
[% "List of $loop.size items:\n----------------\n" IF loop.first %]
 * [% item +%]
[% "----------------\n" IF loop.last  %]
[% END %]
-- expect --
List of 4 items:
----------------
 * alpha
 * bravo
 * charlie
 * delta
----------------

-- test --
[% list = [ a b c d ] %]
[% i = 1 %]
[% FOREACH item = list %]
 #[% i %]/[% list.size %]: [% item +%]
[% i = inc(i) %]
[% END %]
-- expect --
 #1/4: alpha
 #2/4: bravo
 #3/4: charlie
 #4/4: delta

-- test --
[% FOREACH a = ['foo', 'bar', 'baz'] %]
* [% loop.index %] [% a +%]
[% FOREACH b = ['wiz', 'woz', 'waz'] %]
  - [% loop.index %] [% b +%]
[% END %]
[% END %]

-- expect --
* 0 foo
  - 0 wiz
  - 1 woz
  - 2 waz
* 1 bar
  - 0 wiz
  - 1 woz
  - 2 waz
* 2 baz
  - 0 wiz
  - 1 woz
  - 2 waz

-- test --
[% id    = 12345
   name  = 'Original'
   user1 = { id => 'tom', name => 'Thomas'   }
   user2 = { id => 'reg', name => 'Reginald' }
%]
[% FOREACH [ user1 ] %]
  id: [% id +%]
  name: [% name +%]
[% FOREACH [ user2 ] %]
  - id: [% id +%]
  - name: [% name +%]
[% END %]
[% END %]
id: [% id +%]
name: [% name +%]
-- expect --
  id: tom
  name: Thomas
  - id: reg
  - name: Reginald
id: 12345
name: Original

-- test --
[% them = [ people.1 people.2 ] %]
[% "$p.id($p.code): $p.name\n"
       FOREACH p = them.sort('id') %]
-- expect --
aaz(zaz): Azbaz Azbaz Zazbazzer
bcd(dec): Binary Coded Decimal

-- test --
[% "$p.id($p.code): $p.name\n"
       FOREACH p = people.sort('code') %]
-- expect --
abw(abw): Andy Wardley
bcd(dec): Binary Coded Decimal
aaz(zaz): Azbaz Azbaz Zazbazzer
efg(zzz): Extra Fine Grass

-- test --
[% "$p.id($p.code): $p.name\n"
       FOREACH p = people.sort('code').reverse %]
-- expect --
efg(zzz): Extra Fine Grass
aaz(zaz): Azbaz Azbaz Zazbazzer
bcd(dec): Binary Coded Decimal
abw(abw): Andy Wardley

-- test --
[% "$p.id($p.code): $p.name\n"
       FOREACH p = people.sort('code') %]
-- expect --
abw(abw): Andy Wardley
bcd(dec): Binary Coded Decimal
aaz(zaz): Azbaz Azbaz Zazbazzer
efg(zzz): Extra Fine Grass


-- test --
Section List:
[% FOREACH item = sections %]
  [% item.key %] - [% item.value +%]
[% END %]
-- expect --
Section List:
  four - Section Four
  one - Section One
  three - Section Three
  two - Section Two

-- test --
[% FOREACH a = [ 2..6 ] %]
before [% a %]
[% NEXT IF a == 5 +%]
after [% a +%]
[% END %]
-- expect --
before 2
after 2
before 3
after 3
before 4
after 4
before 5before 6
after 6

-- test --
[% count = 1; WHILE (count < 10) %]
[% count = count + 1 %]
[% NEXT IF count < 5 %]
count: [% count +%]
[% END %]
-- expect --
count: 5
count: 6
count: 7
count: 8
count: 9
count: 10

-- test --
[% FOR count = [ 1 2 3 ] %]${count}..[% END %]
-- expect --
1..2..3..

-- test --
[% FOREACH count = [ 1 2 3 ] %]${count}..[% END %]
-- expect --
1..2..3..

-- test --
[% FOR [ 1 2 3 ] %]<blip>..[% END %]
-- expect --
<blip>..<blip>..<blip>..

-- test --
[% FOREACH [ 1 2 3 ] %]<blip>..[% END %]
-- expect --
<blip>..<blip>..<blip>..

-- test -- 
[% FOREACH outer = nested -%]
outer start
[% FOREACH inner = outer -%]
inner [% inner +%]
[% "last inner\n" IF loop.last -%]
[% END %]
[% "last outer\n" IF loop.last -%]
[% END %]
-- expect --
outer start
inner a
inner b
inner c
last inner
outer start
inner x
inner y
inner z
last inner
last outer


-- test --
[% FOREACH n = [ 1 2 3 4 5 ] -%]
[% LAST IF loop.last -%]
[% n %], 
[%- END %]
-- expect --
1, 2, 3, 4, 

-- test --
[% FOREACH n = [ 1 2 3 4 5 ] -%]
[% BREAK IF loop.last -%]
[% n %], 
[%- END %]
-- expect --
1, 2, 3, 4, 

-- test --
-- use debug --
[% FOREACH a = [ 1, 2, 3 ] -%]
* [% a %]
[% END -%]
-- expect --
* 1
* 2
* 3
-- test --
[%
    FOREACH i = [1 .. 10];
        SWITCH i;
        CASE 5;
            NEXT;
        CASE 8;
            LAST;
        END;
        "$i\n";
    END;
-%]
-- expect --
1
2
3
4
6
7
-- test --
[%
    FOREACH i = [1 .. 10];
        IF 1;
            IF i == 5; NEXT; END;
            IF i == 8; LAST; END;
        END;
        "$i\n";
    END;
-%]
-- expect --
1
2
3
4
6
7

-- test --
[%
    FOREACH i = [1 .. 4];
        FOREACH j = [1 .. 4];
            k = 1;
            SWITCH j;
                CASE 2;
                FOREACH k IN [ 1 .. 2 ]; LAST; END;
            CASE 3;
                NEXT IF j == 3;
            END;
            "$i,$j,$k\n";
        END;
    END;
-%]
-- expect --
1,1,1
1,2,1
1,4,1
2,1,1
2,2,1
2,4,1
3,1,1
3,2,1
3,4,1
4,1,1
4,2,1
4,4,1

-- test --
[%
    LAST FOREACH k = [ 1 .. 4];
    "$k\n";
    # Should finish loop with k = 4.  Instead this is an infinite loop!!
    #NEXT FOREACH k = [ 1 .. 4];
    #"$k\n";
-%]
-- expect --
1

-- test --
[% FOREACH prime IN [2, 3, 5, 7, 11, 13];
     "$prime\n";
    END
-%]
-- expect --
2
3
5
7
11
13

-- test --
-- name FOR/WHILE/NEXT --
[%  FOREACH i IN [ 1..6 ];
        "${i}: ";
        j = 0;
        WHILE j < i;
            j = j + 1;
            NEXT IF j > 3;
            "${j} ";
        END;
        "\n";
    END;
%]
-- expect --
1: 1 
2: 1 2 
3: 1 2 3 
4: 1 2 3 
5: 1 2 3 
6: 1 2 3 
