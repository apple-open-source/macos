#============================================================= -*-perl-*-
#
# t/varsv1.t
#
# Template script testing variable use with version 1 compatibility.
# In version 1, leading '$' on variables were ignored.
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
use Template::Constants qw( :status );
$^W = 1;

$Template::Test::DEBUG = 0;

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
    "letter$a" => "'$a'",

    # don't define a 'z' - DEFAULT test relies on its non-existance
};

my $tt = [ default => Template->new({ 
	       INTERPOLATE => 1, 
	       ANYCASE     => 1,
	       V1DOLLAR    => 1,
	   }),
	   notcase => Template->new({ 
	       INTERPOLATE => 1, 
	       V1DOLLAR    => 0,
	   }) ];

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

__DATA__

#------------------------------------------------------------------------
# GET 
#------------------------------------------------------------------------

-- test --
[% a %]
[% $a %]
[% GET b %]
[% GET $b %]
[% get c %]
[% get $c %]
-- expect --
alpha
alpha
bravo
bravo
charlie
charlie

-- test --
[% b %] [% $b %] [% GET b %] [% GET $b %]
-- expect --
bravo bravo bravo bravo

-- test --
$a $b ${c} ${d} [% $e %]
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
[% f.g %] [% $f.g %] [% $f.$g %]
-- expect --
golf golf golf

-- test --
[% GET f.h %] [% get $f.h %] [% get f.${'h'} %] [% get $f.${'h'} %]
-- expect --
hotel hotel hotel hotel

-- test --
$f.h ${f.g} ${f.h}.gif
-- expect --
hotel golf hotel.gif

-- test --
[% f.i.j %] [% $f.i.j %] [% f.$i.j %] [% f.i.$j %] [% $f.$i.$j %]
-- expect --
juliet juliet juliet juliet juliet

-- test --
[% f.i.j %] [% $f.i.j %] [% GET f.i.j %] [% GET $f.i.j %]
-- expect --
juliet juliet juliet juliet

-- test --
[% get $f.i.k %]
-- expect --
kilo

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
[% magic.chant('foo') %] [% GET $magic.chant('foo') %]
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
[% a = $c %] $a
[% $a = d %] $a
[% $a = $e %] $a
-- expect --
 alpha
 bravo
 charlie
 delta
 echo

-- test -- 
[% SET a = a %] $a
[% SET a = b %] $a
[% SET a = $c %] $a
[% SET $a = d %] $a
[% SET $a = $e %] $a
-- expect --
 alpha
 bravo
 charlie
 delta
 echo

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
[% a = f.g %] $a
[% a = $f.h %] $a
[% a = f.i.j %] $a
[% a = $f.i.k %] $a
-- expect --
 golf
 hotel
 juliet
 kilo

-- test --
[% f.g = r %] $f.g
[% $f.h = $r %] $f.h
[% f.i.j = $s %] $f.i.j
[% $f.i.k = f.i.j %] ${f.i.k}
-- expect --
 romeo
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

