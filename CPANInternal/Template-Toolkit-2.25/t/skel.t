#============================================================= -*-perl-*-
#
# t/skel.t
#
# Skeleton test script.
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
$^W = 1;

$Template::Test::DEBUG = 0;
#$Template::Parser::DEBUG = 1;
#$Template::Directive::PRETTY = 1;

ok(1);

my $config = {
    POST_CHOMP => 1,
    EVAL_PERL => 1,
};

my $replace = {
    a => 'alpha',
    b => 'bravo',
};

test_expect(\*DATA, $config, $replace);

__DATA__
# this is the first test
-- test --
[% a %]
-- expect --
alpha

# this is the second test
-- test --
[% b %]
-- expect --
bravo




