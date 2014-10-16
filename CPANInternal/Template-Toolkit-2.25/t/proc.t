#============================================================= -*-perl-*-
#
# t/proc.t
#
# Template script testing the procedural template plugin
#
# Written by Mark Fowler <mark@twoshortplanks.com>
#
# Copyright (C) 2002 Makr Fowler.  All Rights Reserved.
#
# This is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
# $Id$
#
#========================================================================

use strict;
use lib qw( ./lib ../lib t/lib );
use Template::Test;
$^W = 1;

my $ttcfg = {};

test_expect(\*DATA, $ttcfg, &callsign());

__DATA__
-- test --
[% USE ProcFoo -%]
[% ProcFoo.foo %]
[% ProcFoo.bar %]
-- expect --
This is procfoofoo
This is procfoobar
-- test --
[% USE ProcBar -%]
[% ProcBar.foo %]
[% ProcBar.bar %]
[% ProcBar.baz %]
-- expect --
This is procfoofoo
This is procbarbar
This is procbarbaz

