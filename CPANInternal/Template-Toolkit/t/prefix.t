#============================================================= -*-perl-*-
#
# t/prefix.t
#
# Test template prefixes within INCLUDE, etc., directives.
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
use lib qw( ./lib ../lib );
use Template;
use Template::Test;
use Template::Config;
$^W = 1;

#$Template::Test::DEBUG = 0;
#$Template::Context::DEBUG = 0;

# script may be being run in distribution root or 't' directory
my $dir   = -d 't' ? 't/test' : 'test';

my $src_prov = Template::Config->provider( INCLUDE_PATH => "$dir/src" );
my $lib_prov = Template::Config->provider( INCLUDE_PATH => "$dir/lib" );
my $config = {
    LOAD_TEMPLATES => [ $src_prov, $lib_prov ],
    PREFIX_MAP   => {
	src => '0',
	lib => '1',
	all => '0, 1',
    },
};

test_expect(\*DATA, $config);

__DATA__
-- test --
[% INCLUDE foo a=10 %]
-- expect --
This is the foo file, a is 10

-- test --
[% INCLUDE src:foo a=20 %]
-- expect --
This is the foo file, a is 20

-- test --
[% INCLUDE all:foo a=30 %]
-- expect --
This is the foo file, a is 30

-- test --
[% TRY;
    INCLUDE lib:foo a=30 ;
   CATCH;
    error;
   END
%]
-- expect --
file error - lib:foo: not found

-- test --
[% INSERT src:foo %]
-- expect --
This is the foo file, a is [% a -%]
