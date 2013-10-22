#============================================================= -*-perl-*-
#
# t/blocks.t
#
# Test ability to INCLUDE/PROCESS a block in a template.
#
# Written by Andy Wardley <abw@andywardley.com>
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
use Template::Provider;
use Cwd;
$^W = 1;

my $DEBUG = grep(/-d/, @ARGV);
#$Template::Parser::DEBUG = $DEBUG;
#$Template::Directive::PRETTY = $DEBUG;
$Template::Provider::DEBUG = $DEBUG;
#$Template::Context::DEBUG = $DEBUG;

my $path = cwd;
my $dir  = -d 'test/lib' ? "$path/test/lib" : "$path/t/test/lib";

my $tt1 = Template->new({
    INCLUDE_PATH => [ qw( t/test/lib test/lib ) ],
    ABSOLUTE => 1,
});

my $tt2 = Template->new({
    INCLUDE_PATH => [ qw( t/test/lib test/lib ) ],
    EXPOSE_BLOCKS => 1,
    ABSOLUTE => 1,
});

my $vars = {
    a => 'alpha',
    b => 'bravo',
    dir => $dir,
};

test_expect(\*DATA, [ off => $tt1, on => $tt2 ], $vars);

__DATA__
-- test --
[% TRY; INCLUDE blockdef/block1; CATCH; error; END %]

-- expect --
file error - blockdef/block1: not found

-- test --
-- use on --
[% INCLUDE blockdef/block1 %]

-- expect --
This is block 1, defined in blockdef, a is alpha

-- test --
[% INCLUDE blockdef/block1 a='amazing' %]

-- expect --
This is block 1, defined in blockdef, a is amazing

-- test -- 
[% TRY; INCLUDE blockdef/none; CATCH; error; END %]
-- expect --
file error - blockdef/none: not found

-- test --
[% INCLUDE "$dir/blockdef/block1" a='abstract' %]

-- expect --
This is block 1, defined in blockdef, a is abstract

-- test --
[% BLOCK one -%]
block one
[% BLOCK two -%]
this is block two, b is [% b %]
[% END -%]
two has been defined, let's now include it
[% INCLUDE one/two b='brilliant' -%]
end of block one
[% END -%]
[% INCLUDE one -%]
=
[% INCLUDE one/two b='brazen'-%]
--expect --
block one
two has been defined, let's now include it
this is block two, b is brilliant
end of block one
=
this is block two, b is brazen
