#============================================================= -*-perl-*-
#
# t/strcat.t
#
# Test the string concatenation operator ' _ '.
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

test_expect(\*DATA);

__DATA__
-- test --
[% foo = 'the foo string'
   bar = 'the bar string'
   baz = foo _ ' and ' _ bar
-%]
baz: [% baz %]
-- expect --
baz: the foo string and the bar string

