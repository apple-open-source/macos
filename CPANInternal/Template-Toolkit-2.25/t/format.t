#============================================================= -*-perl-*-
#
# t/format.t
#
# Template script testing the format plugin.
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
use Template qw( :status );
use Template::Test;
$^W = 1;

$Template::Test::DEBUG = 0;
$Template::Test::PRESERVE = 1;

my ($a, $b, $c, $d) = qw( alpha bravo charlie delta );
my $params = { 
    'a'      => $a,
    'b'      => $b,
    'c'      => $c,
    'd'      => $d,
};

test_expect(\*DATA, { INTERPOLATE => 1, POST_CHOMP => 1 }, $params);
 

#------------------------------------------------------------------------
# test input
#------------------------------------------------------------------------

__DATA__
[% USE format %]
[% bold = format('<b>%s</b>') %]
[% ital = format('<i>%s</i>') %]
[% bold('heading') +%]
[% ital('author')  +%]
${ ital('affil.') }
[% bold('footing')  +%]
$bold

-- expect --
<b>heading</b>
<i>author</i>
<i>affil.</i>
<b>footing</b>
<b></b>

-- test --
[% USE format('<li> %s') %]
[% FOREACH item = [ a b c d ] %]
[% format(item) +%]
[% END %]
-- expect --
<li> alpha
<li> bravo
<li> charlie
<li> delta

-- test --
[% USE bold = format("<b>%s</b>") %]
[% USE ital = format("<i>%s</i>") %]
[% bold('This is bold')   +%]
[% ital('This is italic') +%]
-- expect --
<b>This is bold</b>
<i>This is italic</i>

-- test --
[% USE padleft  = format('%-*s') %]
[% USE padright = format('%*s')  %]
[% padleft(10, a) %]-[% padright(10, b) %]

-- expect --
alpha     -     bravo

