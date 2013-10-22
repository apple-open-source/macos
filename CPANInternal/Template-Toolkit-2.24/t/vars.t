#============================================================= -*-perl-*-
#
# t/vars.t
#
# Template script testing variable use.
#
# Written by Andy Wardley <abw@wardley.org>
#
# Copyright (C) 1996-2006 Andy Wardley.  All Rights Reserved.
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
use Template::Test;
use Template::Stash;
use Template::Constants qw( :status );
use Template::Directive;
use Template::Parser;
$Template::Test::DEBUG = 0;
$Template::Parser::DEBUG = 0;

# sample data
my ($a, $b, $c, $d, $e, $f, $g, $h, $i, $j, $k, $l, $m, 
    $n, $o, $p, $q, $r, $s, $t, $u, $v, $w, $x, $y, $z) = 
    qw( alpha bravo charlie delta echo foxtrot golf hotel india 
        juliet kilo lima mike november oscar papa quebec romeo 
        sierra tango umbrella victor whisky x-ray yankee zulu );

my @days   = qw( Monday Tuesday Wednesday Thursday Friday Saturday Sunday );
my $day    = -1;
my $count  = 0;
my $params = { 
    'a' => $a,
    'b' => $b,
    'c' => $c,
    'd' => $d,
    'e' => $e,
    'f' => {
        'g' => $g,
        'h' => $h,
        'i' => {
            'j' => $j,
            'k' => $k,
        },
    },
    'g' => "solo $g",
    'l' => $l,
    'r' => $r,
    's' => $s,
    't' => $t,
    'w' => $w,
    'n'      => sub { $count },
    'up'     => sub { ++$count },
    'down'   => sub { --$count },
    'reset'  => sub { $count = shift(@_) || 0 },
    'undef'  => sub { undef },
    'zero'   => sub { 0 },
    'one'    => sub { 'one' },
    'halt'   => sub { die Template::Exception->new('stop', 'stopped') },
    'join'   => sub { join(shift, @_) },
    'split'  => sub { my $s = shift; $s = quotemeta($s); 
                      my @r = split(/$s/, shift); \@r },
    'magic'  => {
        'chant' => 'Hocus Pocus',
        'spell' => sub { join(" and a bit of ", @_) },
    }, 
    'day'    => {
        'prev' => \&yesterday,
        'this' => \&today,
        'next' => \&tomorrow,
    },
    'belief'   => \&belief,
    'people'   => sub { return qw( Tom Dick Larry ) },
    'gee'      =>  'g',
    "letter$a" => "'$a'",
    'yankee'   => \&yankee,
    '_private' => 123,
    '_hidden'  => 456,
    expose     => sub { undef $Template::Stash::PRIVATE },
    add        => sub { $_[0] + $_[1] },

    # don't define a 'z' - DEFAULT test relies on its non-existance
};

my $tt = [ default => Template->new({ INTERPOLATE => 1, ANYCASE => 1 }),
           notcase => Template->new({ INTERPOLATE => 1, ANYCASE => 0 }) ];

test_expect(\*DATA, $tt, $params);


#------------------------------------------------------------------------
# subs 
#------------------------------------------------------------------------

sub yesterday {
    return "All my troubles seemed so far away...";
}

sub today {
    my $when = shift || 'Now';
    return "$when it looks as though they're here to stay.";
}

sub tomorrow {
    my $dayno = shift;
    unless (defined $dayno) {
    $day++;
    $day %= 7;
    $dayno = $day;
    }
    return $days[$dayno];
}

sub belief {
    my @beliefs = @_;
    my $b = join(' and ', @beliefs);
    $b = '<nothing>' unless length $b;
    return "Oh I believe in $b.";
}

sub yankee {
    my $a = [];
    $a->[1] = { a => 1 };
    $a->[3] = { a => 2 };
    return $a;
}

__DATA__

#------------------------------------------------------------------------
# GET 
#------------------------------------------------------------------------

-- test --
[[% nosuchvariable %]]
[$nosuchvariable]
-- expect --
[]
[]

-- test --
[% a %]
[% GET b %]
[% get c %]
-- expect --
alpha
bravo
charlie

-- test --
[% b %] [% GET b %]
-- expect --
bravo bravo

-- test --
$a $b ${c} ${d} [% e %]
-- expect --
alpha bravo charlie delta echo

-- test --
[% letteralpha %]
[% ${"letter$a"} %]
[% GET ${"letter$a"} %]
-- expect --
'alpha'
'alpha'
'alpha'

-- test --
[% f.g %] [% f.$gee %] [% f.${gee} %]
-- expect --
golf golf golf

-- test --
[% GET f.h %] [% get f.h %] [% f.${'h'} %] [% get f.${'h'} %]
-- expect --
hotel hotel hotel hotel

-- test --
$f.h ${f.g} ${f.h}.gif
-- expect --
hotel golf hotel.gif

-- test --
[% f.i.j %] [% GET f.i.j %] [% get f.i.k %]
-- expect --
juliet juliet kilo

-- test --
[% f.i.j %] $f.i.k [% f.${'i'}.${"j"} %] ${f.i.k}.gif
-- expect --
juliet kilo juliet kilo.gif

-- test --
[% 'this is literal text' %]
[% GET 'so is this' %]
[% "this is interpolated text containing $r and $f.i.j" %]
[% GET "$t?" %]
[% "<a href=\"${f.i.k}.html\">$f.i.k</a>" %]
-- expect --
this is literal text
so is this
this is interpolated text containing romeo and juliet
tango?
<a href="kilo.html">kilo</a>

-- test --
[% name = "$a $b $w" -%]
Name: $name
-- expect --
Name: alpha bravo whisky

-- test --
[% join('--', a b, c, f.i.j) %]
-- expect --
alpha--bravo--charlie--juliet

-- test --
[% text = 'The cat sat on the mat' -%]
[% FOREACH word = split(' ', text) -%]<$word> [% END %]
-- expect --
<The> <cat> <sat> <on> <the> <mat> 

-- test -- 
[% magic.chant %] [% GET magic.chant %]
[% magic.chant('foo') %] [% GET magic.chant('foo') %]
-- expect --
Hocus Pocus Hocus Pocus
Hocus Pocus Hocus Pocus

-- test -- 
<<[% magic.spell %]>>
[% magic.spell(a b c) %]
-- expect --
<<>>
alpha and a bit of bravo and a bit of charlie

-- test --
[% one %] [% one('two', 'three') %] [% one(2 3) %]
-- expect --
one one one

-- test --
[% day.prev %]
[% day.this %]
[% belief('yesterday') %]
-- expect --
All my troubles seemed so far away...
Now it looks as though they're here to stay.
Oh I believe in yesterday.

-- test --
Yesterday, $day.prev
$day.this
${belief('yesterday')}
-- expect --
Yesterday, All my troubles seemed so far away...
Now it looks as though they're here to stay.
Oh I believe in yesterday.

-- test --
-- use notcase --
[% day.next %]
$day.next
-- expect --
Monday
Tuesday

-- test --
[% FOREACH [ 1 2 3 4 5 ] %]$day.next [% END %]
-- expect --
Wednesday Thursday Friday Saturday Sunday 

-- test --
-- use default --
before
[% halt %]
after

-- expect --
before

-- test --
[% FOREACH k = yankee -%]
[% loop.count %]. [% IF k; k.a; ELSE %]undef[% END %]
[% END %]
-- expect --
1. undef
2. 1
3. undef
4. 2


#------------------------------------------------------------------------
# CALL 
#------------------------------------------------------------------------

-- test --
before [% CALL a %]a[% CALL b %]n[% CALL c %]d[% CALL d %] after
-- expect --
before and after

-- test --
..[% CALL undef %]..
-- expect --
....

-- test --
..[% CALL zero %]..
-- expect --
....

-- test --
..[% n %]..[% CALL n %]..
-- expect --
..0....

-- test --
..[% up %]..[% CALL up %]..[% n %]
-- expect --
..1....2

-- test --
[% CALL reset %][% n %]
-- expect --
0

-- test --
[% CALL reset(100) %][% n %]
-- expect --
100

#------------------------------------------------------------------------
# SET 
#------------------------------------------------------------------------

-- test --
[% a = a %] $a
[% a = b %] $a
-- expect --
 alpha
 bravo

-- test -- 
[% SET a = a %] $a
[% SET a = b %] $a
[% SET a = $c %] [$a]
[% SET a = $gee %] $a
[% SET a = ${gee} %] $a
-- expect --
 alpha
 bravo
 []
 solo golf
 solo golf

-- test --
[% a = b
   b = c
   c = d
   d = e
%][% a %] [% b %] [% c %] [% d %]
-- expect --
bravo charlie delta echo

-- test --
[% SET
   a = c
   b = d
   c = e
%]$a $b $c
-- expect --
charlie delta echo

-- test --
[% 'a' = d
   'include' = e
   'INCLUDE' = f.g
%][% a %]-[% ${'include'} %]-[% ${'INCLUDE'} %]
-- expect --
delta-echo-golf

-- test --
[% a = f.g %] $a
[% a = f.i.j %] $a
-- expect --
 golf
 juliet

-- test --
[% f.g = r %] $f.g
[% f.i.j = s %] $f.i.j
[% f.i.k = f.i.j %] ${f.i.k}
-- expect --
 romeo
 sierra
 sierra

-- test --
[% user = {
    id = 'abw'
    name = 'Andy Wardley'
    callsign = "[-$a-$b-$w-]"
   }
-%]
${user.id} ${ user.id } $user.id ${user.id}.gif
[% message = "$b: ${ user.name } (${user.id}) ${ user.callsign }" -%]
MSG: $message
-- expect --
abw abw abw abw.gif
MSG: bravo: Andy Wardley (abw) [-alpha-bravo-whisky-]

-- test --
[% product = {
     id   => 'XYZ-2000',
     desc => 'Bogon Generator',
     cost => 678,
   }
-%]
The $product.id $product.desc costs \$${product.cost}.00
-- expect --
The XYZ-2000 Bogon Generator costs $678.00

-- test --
[% data => {
       g => 'my data'
   }
   complex = {
       gee => 'g'
   }
-%]
[% data.${complex.gee} %]
-- expect --
my data


#------------------------------------------------------------------------
# DEFAULT
#------------------------------------------------------------------------

-- test --
[% a %]
[% DEFAULT a = b -%]
[% a %]
-- expect --
alpha
alpha

-- test --
[% a = '' -%]
[% DEFAULT a = b -%]
[% a %]
-- expect --
bravo

-- test --
[% a = ''   b = '' -%]
[% DEFAULT 
   a = c
   b = d
   z = r
-%]
[% a %] [% b %] [% z %]
-- expect --
charlie delta romeo


#------------------------------------------------------------------------
# 'global' vars
#------------------------------------------------------------------------

-- test --
[% global.version = '3.14' -%]
Version: [% global.version %]
-- expect --
Version: 3.14

-- test --
Version: [% global.version %]
-- expect --
Version: 3.14

-- test --
[% global.newversion = global.version + 1 -%]
Version: [% global.version %]
Version: [% global.newversion %]
-- expect --
Version: 3.14
Version: 4.14

-- test --
Version: [% global.version %]
Version: [% global.newversion %]
-- expect --
Version: 3.14
Version: 4.14

-- test --
[% hash1 = {
      foo => 'Foo',
      bar => 'Bar',
   }
   hash2 = {
      wiz => 'Wiz',
      woz => 'Woz',
   }
-%]
[% hash1.import(hash2) -%]
keys: [% hash1.keys.sort.join(', ') %]
-- expect --
keys: bar, foo, wiz, woz

-- test --
[% mage = { name    =>    'Gandalf', 
        aliases =>  [ 'Mithrandir', 'Olorin', 'Incanus' ] }
-%]
[% import(mage) -%]
[% name %]
[% aliases.join(', ') %]
-- expect --
Gandalf
Mithrandir, Olorin, Incanus


# test private variables
-- test --
[[% _private %]][[% _hidden %]]
-- expect --
[][]

# make them visible
-- test --
[% CALL expose -%]
[[% _private %]][[% _hidden %]]
-- expect --
[123][456]



# Stas reported a problem with spacing in expressions but I can't
# seem to reproduce it...
-- test --
[% a = 4 -%]
[% b=6 -%]
[% c = a + b -%]
[% d=a+b -%]
[% c %]/[% d %]
-- expect --
10/10

-- test --
[% a = 1
   b = 2
   c = 3
-%]
[% d = 1+1 %]d: [% d %]
[% e = a+b %]e: [% e %]
-- expect --
d: 2
e: 3


# these tests check that the incorrect precedence in the parser has now
# been fixed, thanks to Craig Barrat.
-- test --
[%  1 || 0 && 0  # should be 1 || (0&&0), not (1||0)&&0 %]
-- expect --
1

-- test --
[%  1 + !0 + 1  # should be 1 + (!0) + 0, not 1 + !(0 + 1) %]
-- expect --
3

-- test --
[% "x" _ "y" == "y"; ','  # should be ("x"_"y")=="y", not "x"_("y"=="y") %]
-- expect --
,

-- test --
[% "x" _ "y" == "xy"      # should be ("x"_"y")=="xy", not "x"_("y"=="xy") %]
-- expect --
1

-- test --
[% add(3, 5) %]
-- expect --
8

-- test --
[% add(3 + 4, 5 + 7) %]
-- expect --
19

-- test --
[% a = 10;
   b = 20;
   c = 30;
   add(add(a,b+1),c*3);
%]
-- expect --
121

-- test --
[% a = 10;
   b = 20;
   c = 30;
   d = 5;
   e = 7;
   add(a+5, b < 10 ? c : d + e*5);
-%]
-- expect --
55

