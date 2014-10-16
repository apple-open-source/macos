#============================================================= -*-perl-*-
#
# t/process.t
#
# Test the PROCESS option.
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
use Template::Service;

my $dir    = -d 't' ? 't/test' : 'test';
my $config = {
    INCLUDE_PATH => "$dir/src:$dir/lib",
    PROCESS      => 'content',
    TRIM         => 1,
};
my $tt1 = Template->new($config);

$config->{ PRE_PROCESS  } = 'config';
$config->{ PROCESS      } = 'header:content';
$config->{ POST_PROCESS } = 'footer';
$config->{ TRIM } = 0;
my $tt2 = Template->new($config);

$config->{ PRE_PROCESS } = 'config:header.tt2';
$config->{ PROCESS } = '';
my $tt3 = Template->new($config);

my $replace = {
    title => 'Joe Random Title',
};


test_expect(\*DATA, [ tt1 => $tt1, tt2 => $tt2, tt3 => $tt3 ], $replace);

__END__
-- test --
This is the first test
-- expect --
This is the main content wrapper for "untitled"
This is the first test
This is the end.

-- test --
[% META title = 'Test 2' -%]
This is the second test
-- expect --
This is the main content wrapper for "Test 2"
This is the second test
This is the end.

-- test --
-- use tt2 --
[% META title = 'Test 3' -%]
This is the third test
-- expect --
header:
  title: Joe Random Title
  menu: This is the menu, defined in 'config'
This is the main content wrapper for "Test 3"
This is the third test

This is the end.
footer

-- test --
-- use tt3 --
[% META title = 'Test 3' -%]
This is the third test
-- expect --
header.tt2:
  title: Joe Random Title
  menu: This is the menu, defined in 'config'
footer

