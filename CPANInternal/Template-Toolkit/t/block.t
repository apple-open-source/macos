#============================================================= -*-perl-*-
#
# t/block.t
#
# Template script testing BLOCK definitions.  A BLOCK defined in a 
# template incorporated via INCLUDE should not be visible (i.e. 
# exported) to the calling template.  In the same case for PROCESS,
# the block should become visible.
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

my $ttcfg = {
    INCLUDE_PATH => [ qw( t/test/lib test/lib ) ],	
    POST_CHOMP   => 1,
    BLOCKS       => {
	header   => '<html><head><title>[% title %]</title></head><body>',
	footer   => '</body></html>',
	block_a  => sub { return 'this is block a' },
	block_b  => sub { return 'this is block b' },
    },
};

test_expect(\*DATA, $ttcfg, &callsign);

__DATA__

-- test --
[% BLOCK block1 %]
This is the original block1
[% END %]
[% INCLUDE block1 %]
[% INCLUDE blockdef %]
[% INCLUDE block1 %]

-- expect --
This is the original block1
start of blockdef
end of blockdef
This is the original block1

-- test --
[% BLOCK block1 %]
This is the original block1
[% END %]
[% INCLUDE block1 %]
[% PROCESS blockdef %]
[% INCLUDE block1 %]

-- expect --
This is the original block1
start of blockdef
end of blockdef
This is block 1, defined in blockdef, a is alpha

-- test --
[% INCLUDE block_a +%]
[% INCLUDE block_b %]
-- expect --
this is block a
this is block b

-- test --
[% INCLUDE header 
   title = 'A New Beginning'
+%]
A long time ago in a galaxy far, far away...
[% PROCESS footer %]

-- expect --
<html><head><title>A New Beginning</title></head><body>
A long time ago in a galaxy far, far away...
</body></html>

-- test --
[% BLOCK foo:bar %]
blah
[% END %]
[% PROCESS foo:bar %]
-- expect --
blah

-- test --
[% BLOCK 'hello html' -%]
Hello World!
[% END -%]
[% PROCESS 'hello html' %]
-- expect --
Hello World!

-- test --
<[% INCLUDE foo %]>
[% BLOCK foo %][% END %]
-- expect --
<>

-- stop --
# these test the experimental BLOCK args feature which will hopefully allow
# parser/eval options to be set for different blocks

-- test --
[% BLOCK foo eval_perl=0 tags="star" -%]
This is the foo block
[% END -%]
foo: [% INCLUDE foo %]
-- expect --
foo: This is the foo block

-- test --
[% BLOCK eval_perl=0 tags="star" -%]
This is an anonymous block
[% END -%]
-- expect --
This is an anonymous block

