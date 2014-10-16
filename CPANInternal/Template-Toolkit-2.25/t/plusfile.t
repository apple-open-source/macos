#============================================================= -*-perl-*-
#
# t/plufile.t
#
# Test ability to specify INCLUDE/PROCESS/WRAPPER files in the 
# form "foo+bar+baz".
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
use Template;
use Template::Test;
use Template::Context;
$^W = 1;

#$Template::Parser::DEBUG = 1;
#$Template::Directive::PRETTY = 1;
$Template::Test::PRESERVE = 1;

my $dir = -d 't' ? 't/test/src' : 'test/src';

test_expect(\*DATA, { INCLUDE_PATH => $dir  });

__DATA__
-- test --
[% INCLUDE foo %]
[% BLOCK foo; "This is foo!"; END %]
-- expect --
This is foo!

-- test --
[% INCLUDE foo+bar -%]
[% BLOCK foo; "This is foo!\n"; END %]
[% BLOCK bar; "This is bar!\n"; END %]
-- expect --
This is foo!
This is bar!

-- test --
[% PROCESS foo+bar -%]
[% BLOCK foo; "This is foo!\n"; END %]
[% BLOCK bar; "This is bar!\n"; END %]
-- expect --
This is foo!
This is bar!

-- test --
[% WRAPPER edge + box + indent
     title = "The Title" -%]
My content
[% END -%]
[% BLOCK indent -%]
<indent>
[% content -%]
</indent>
[% END -%]
[% BLOCK box -%]
<box>
[% content -%]
</box>
[% END -%]
[% BLOCK edge -%]
<edge>
[% content -%]
</edge>
[% END -%]
-- expect --
<edge>
<box>
<indent>
My content
</indent>
</box>
</edge>


-- test --
[% INSERT foo+bar/baz %]
-- expect --
This is the foo file, a is [% a -%][% DEFAULT word = 'qux' -%]
This is file baz
The word is '[% word %]'

-- test --
[% file1 = 'foo'
   file2 = 'bar/baz'
-%]
[% INSERT "$file1" + "$file2" %]
-- expect --
This is the foo file, a is [% a -%][% DEFAULT word = 'qux' -%]
This is file baz
The word is '[% word %]'

