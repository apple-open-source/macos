#============================================================= -*-perl-*-
#
# t/wrap.t
#
# Template script testing wrap plugin.
#
# Written by Andy Wardley <abw@kfs.org>
#
# Copyright (C) 1996-2000 Andy Wardley.  All Rights Reserved.
#
# This is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
# $Id$
#
#========================================================================

use strict;
use lib qw( ../lib );
use Template qw( :status );
use Template::Test;
$^W = 1;

$Template::Test::DEBUG = 0;
#$Template::Parser::DEBUG = 1;
#$Template::Directive::PRETTY = 1;

eval "use Text::Wrap";

if ($@) {
    skip_all('Text::Wrap not installed');
}

test_expect(\*DATA);
 

#------------------------------------------------------------------------
# test input
#------------------------------------------------------------------------

__DATA__
-- test --
[% USE Wrap -%]
[% text = BLOCK -%]
This is a long block of text that goes on for a long long time and then carries on some more after that, it's very interesting, NOT!
[%- END -%]
[% text = BLOCK; text FILTER replace('\s+', ' '); END -%]
[% Wrap(text, 25,) %]
-- expect --
This is a long block of
text that goes on for a
long long time and then
carries on some more
after that, it's very
interesting, NOT!

-- test --
[% FILTER wrap -%]
This is a long block of text that goes on for a long long time and then carries on some more after that, it's very interesting, NOT!
[% END %]
-- expect --
This is a long block of text that goes on for a long long time and then
carries on some more after that, it's very interesting, NOT!

-- test --
[% USE wrap -%]
[% FILTER wrap(25) -%]
This is a long block of text that goes on for a long long time and then carries on some more after that, it's very interesting, NOT!
[% END %]
-- expect --
This is a long block of
text that goes on for a
long long time and then
carries on some more
after that, it's very
interesting, NOT!

-- test --
[% FILTER wrap(10, '> ', '+ ') -%]
The cat sat on the mat and then sat on the flat.
[%- END %]
-- expect --
> The cat
+ sat on
+ the mat
+ and
+ then
+ sat on
+ the
+ flat.

-- test --
[% USE wrap -%]
[% FILTER bullet = wrap(40, '* ', '  ') -%]
First, attach the transmutex multiplier to the cross-wired quantum
homogeniser.
[%- END %]
[% FILTER remove('\s+(?=\n)') -%]
[% FILTER bullet -%]
Then remodulate the shield to match the harmonic frequency, taking 
care to correct the phase difference.
[% END %]
[% END %]
-- expect --
* First, attach the transmutex
  multiplier to the cross-wired quantum
  homogeniser.
* Then remodulate the shield to match
  the harmonic frequency, taking
  care to correct the phase difference.
