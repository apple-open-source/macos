#============================================================= -*-perl-*-
#
# t/list.t
#
# Tests list references as variables, including pseudo-methods such
# as first(), last(), etc.
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

use Template::Parser;
$Template::Test::DEBUG = 0;
#$Template::Parser::DEBUG = 1;
#$Template::Directive::PRETTY = 1;

# sample data
my ($a, $b, $c, $d, $e, $f, $g, $h, $i, $j, $k, $l, $m, 
    $n, $o, $p, $q, $r, $s, $t, $u, $v, $w, $x, $y, $z) = 
	qw( alpha bravo charlie delta echo foxtrot golf hotel india 
	    juliet kilo lima mike november oscar papa quebec romeo 
	    sierra tango umbrella victor whisky x-ray yankee zulu );

my $data = [ $r, $j, $s, $t, $y, $e, $f, $z ];
my $vars = { 
    'a'  => $a,
    'b'  => $b,
    'c'  => $c,
    'd'  => $d,
    'e'  => $e,
    data => $data,
    days => [ qw( Mon Tue Wed Thu Fri Sat Sun ) ],
    wxyz => [ { id => $z, name => 'Zebedee', rank => 'aa' },
	      { id => $y, name => 'Yinyang', rank => 'ba' },
	      { id => $x, name => 'Xeexeez', rank => 'ab' },
	      { id => $w, name => 'Warlock', rank => 'bb' }, ],
    inst => [ { name => 'piano', url => '/roses.html'  },
	      { name => 'flute', url => '/blow.html'   },
	      { name => 'organ', url => '/tulips.html' }, ],
    nest => [ [ 3, 1, 4 ], [ 2, [ 7, 1, 8 ] ] ],
};

my $config = {};

test_expect(\*DATA, $config, $vars);


__DATA__

#------------------------------------------------------------------------
# GET 
#------------------------------------------------------------------------
-- test --
[% data.0 %] and [% data.1 %]
-- expect --
romeo and juliet

-- test --
[% data.first %] - [% data.last %]
-- expect --
romeo - zulu

-- test --
[% data.size %] [% data.max %]
-- expect --
8 7

-- test --
[% data.join(', ') %]
-- expect --
romeo, juliet, sierra, tango, yankee, echo, foxtrot, zulu

-- test --
[% data.reverse.join(', ') %]
-- expect --
zulu, foxtrot, echo, yankee, tango, sierra, juliet, romeo

-- test --
[% data.sort.reverse.join(' - ') %]
-- expect --
zulu - yankee - tango - sierra - romeo - juliet - foxtrot - echo

-- test --
[% FOREACH item = wxyz.sort('id') -%]
* [% item.name %]
[% END %]
-- expect --
* Warlock
* Xeexeez
* Yinyang
* Zebedee

-- test --
[% FOREACH item = wxyz.sort('rank') -%]
* [% item.name %]
[% END %]
-- expect --
* Zebedee
* Xeexeez
* Yinyang
* Warlock

-- test --
[% FOREACH n = [0..6] -%]
[% days.$n +%]
[% END -%]
-- expect --
Mon
Tue
Wed
Thu
Fri
Sat
Sun

-- test --
[% data = [ 'one', 'two', data.first ] -%]
[% data.join(', ') %]
-- expect --
one, two, romeo

-- test --
[% data = [ 90, 8, 70, 6, 1, 11, 10, 2, 5, 50, 52 ] -%]
 sort: [% data.sort.join(', ') %]
nsort: [% data.nsort.join(', ') %]
-- expect --
 sort: 1, 10, 11, 2, 5, 50, 52, 6, 70, 8, 90
nsort: 1, 2, 5, 6, 8, 10, 11, 50, 52, 70, 90

-- test --
[% ilist = [] -%]
[% ilist.push("<a href=\"$i.url\">$i.name</a>") FOREACH i = inst -%]
[% ilist.join(",\n") -%]
[% global.ilist = ilist -%]
-- expect --
<a href="/roses.html">piano</a>,
<a href="/blow.html">flute</a>,
<a href="/tulips.html">organ</a>

-- test -- 
[% global.ilist.pop %]
-- expect --
<a href="/tulips.html">organ</a>

-- test -- 
[% global.ilist.shift %]
-- expect --
<a href="/roses.html">piano</a>

-- test -- 
[% global.ilist.unshift('another') -%]
[% global.ilist.join(', ') %]
-- expect --
another, <a href="/blow.html">flute</a>

-- test --
[% nest.0.0 %].[% nest.0.1 %][% nest.0.2 +%]
[% nest.1.shift %].[% nest.1.0.join('') %]
-- expect --
3.14
2.718

-- test --
[% # define some initial data
   people   => [ 
     { id => 'tom',   name => 'Tom'     },
     { id => 'dick',  name => 'Richard' },
     { id => 'larry', name => 'Larry'   },
   ]
-%]
[% folk = [] -%]
[% folk.push("<a href=\"${person.id}.html\">$person.name</a>")
       FOREACH person = people.sort('name') -%]
[% folk.join(",\n") -%]
-- expect --
<a href="larry.html">Larry</a>,
<a href="dick.html">Richard</a>,
<a href="tom.html">Tom</a>

-- test --
[% data.grep('r').join(', ') %]
-- expect --
romeo, sierra, foxtrot

-- test --
[% data.grep('^r').join(', ') %]
-- expect --
romeo
