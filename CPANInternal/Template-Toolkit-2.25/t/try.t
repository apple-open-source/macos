#============================================================= -*-perl-*-
#
# t/try.t
#
# Template script testing TRY / THROW / CATCH / FINAL blocks.
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
};
my $replace = &callsign();
$replace->{ throw_egg } = sub { 
    die (Template::Exception->new('egg', 'scrambled'));
};
$replace->{ throw_any } = sub { 
    die "undefined error\n";
};

test_expect(\*DATA, $ttcfg, $replace);

__DATA__

#------------------------------------------------------------------------
# throw default type
#------------------------------------------------------------------------
-- test --
[% TRY %]
[% THROW foxtrot %]
[% CATCH %]
[[% error.type%]] [% error.info %]
[% END %]
-- expect --
[undef] foxtrot

-- test --
[% TRY %]
[% THROW $f %]
[% CATCH %]
[[% error.type%]] [% error.info %]
[% END %]
-- expect --
[undef] foxtrot

#------------------------------------------------------------------------
# throw simple types
#------------------------------------------------------------------------
-- test --
before try
[% TRY %]
try this
[% THROW barf "Feeling sick" %]
don't try this
[% CATCH barf %]
caught barf: [% error.info +%]
[% END %]
after try

-- expect --
before try
try this
caught barf: Feeling sick
after try

-- test --
before
[% TRY %]
some content
[% THROW up 'more malaise' %]
afterthought
[% CATCH barf %]
no barf
[% CATCH up %]
caught up: [% error.info +%]
[% CATCH %]
no default
[% END %]
after
-- expect --
before
some content
caught up: more malaise
after

-- test --
before
[% TRY %]
some content
[% THROW up b %]
afterthought
[% CATCH barf %]
no barf
[% CATCH up %]
caught up: [% error.info +%]
[% CATCH %]
no default
[% END %]
after
-- expect --
before
some content
caught up: bravo
after

-- test --
before
[% TRY %]
some content
[% THROW $a b %]
afterthought
[% CATCH barf %]
no barf
[% CATCH up %]
caught up: [% error.info +%]
[% CATCH alpha %]
caught up: [% error.info +%]
[% CATCH %]
no default
[% END %]
after
-- expect --
before
some content
caught up: bravo
after

#------------------------------------------------------------------------
# throw complex (hierarchical) exception types
#------------------------------------------------------------------------
-- test --
before
[% TRY %]
some content
[% THROW alpha.bravo c %]
afterthought
[% CATCH alpha.charlie %]
WRONG: [% error.info +%]
[% CATCH alpha.bravo %]
RIGHT: [% error.info +%]
[% CATCH alpha %]
WRONG: [% error.info +%]
[% CATCH %]
WRONG: [% error.info +%]
[% END %]
after
-- expect --
before
some content
RIGHT: charlie
after

-- test --
before
[% TRY %]
some content
[% THROW alpha.bravo c %]
afterthought
[% CATCH delta.charlie %]
WRONG: [% error.info +%]
[% CATCH delta.bravo %]
WRONG: [% error.info +%]
[% CATCH alpha %]
RIGHT: [% error.info +%]
[% CATCH %]
WRONG: [% error.info +%]
[% END %]
after
-- expect --
before
some content
RIGHT: charlie
after

-- test --
before
[% TRY %]
some content
[% THROW "alpha.$b" c %]
afterthought
[% CATCH delta.charlie %]
WRONG: [% error.info +%]
[% CATCH alpha.bravo %]
RIGHT: [% error.info +%]
[% CATCH alpha.charlie %]
WRONG: [% error.info +%]
[% CATCH %]
WRONG: [% error.info +%]
[% END %]
after
-- expect --
before
some content
RIGHT: charlie
after

-- test --
before
[% TRY %]
some content
[% THROW alpha.bravo c %]
afterthought
[% CATCH delta.charlie %]
WRONG: [% error.info +%]
[% CATCH delta.bravo %]
WRONG: [% error.info +%]
[% CATCH alpha.charlie %]
WRONG: [% error.info +%]
[% CATCH %]
RIGHT: [% error.info +%]
[% END %]
after
-- expect --
before
some content
RIGHT: charlie
after

-- test --
before
[% TRY %]
some content
[% THROW alpha.bravo.charlie d %]
afterthought
[% CATCH alpha.bravo.charlie %]
RIGHT: [% error.info +%]
[% CATCH alpha.bravo %]
WRONG: [% error.info +%]
[% CATCH alpha %]
WRONG: [% error.info +%]
[% CATCH %]
WRONG: [% error.info +%]
[% END %]
after
-- expect --
before
some content
RIGHT: delta
after

-- test --
before
[% TRY %]
some content
[% THROW alpha.bravo.charlie d %]
afterthought
[% CATCH alpha.bravo.foxtrot %]
WRONG: [% error.info +%]
[% CATCH alpha.bravo %]
RIGHT: [% error.info +%]
[% CATCH alpha %]
WRONG: [% error.info +%]
[% CATCH %]
WRONG: [% error.info +%]
[% END %]
after
-- expect --
before
some content
RIGHT: delta
after

-- test --
before
[% TRY %]
some content
[% THROW alpha.bravo.charlie d %]
afterthought
[% CATCH alpha.bravo.foxtrot %]
WRONG: [% error.info +%]
[% CATCH alpha.echo %]
WRONG: [% error.info +%]
[% CATCH alpha %]
RIGHT: [% error.info +%]
[% CATCH %]
WRONG: [% error.info +%]
[% END %]
after
-- expect --
before
some content
RIGHT: delta
after

#------------------------------------------------------------------------
# test FINAL block
#------------------------------------------------------------------------
-- test --
[% TRY %]
foo
[% CATCH %]
bar
[% FINAL %]
baz
[% END %]
-- expect --
foo
baz

-- test --
[% TRY %]
foo
[% THROW anything %]
[% CATCH %]
bar
[% FINAL %]
baz
[% END %]
-- expect --
foo
bar
baz

#------------------------------------------------------------------------
# use CLEAR to clear output from TRY block
#------------------------------------------------------------------------
-- test --
before
[% TRY %]
foo
[% THROW anything %]
[% CATCH %]
[% CLEAR %]
bar
[% FINAL %]
baz
[% END %]
-- expect --
before
bar
baz

-- test --
before
[% TRY %]
foo
[% CATCH %]
bar
[% FINAL %]
[% CLEAR %]
baz
[% END %]
-- expect --
before
baz


#------------------------------------------------------------------------
# nested TRY blocks
#------------------------------------------------------------------------
-- test --
before
[% TRY %]
outer
[% TRY %]
inner
[% THROW foo g %]
more inner
[% CATCH %]
caught inner
[% END %]
more outer
[% CATCH %]
caught outer
[% END %]
after
-- expect --
before
outer
inner
caught inner
more outer
after

-- test --
before
[% TRY %]
outer
[% TRY %]
inner
[% THROW foo g %]
more inner
[% CATCH foo %]
caught inner foo
[% CATCH %]
caught inner
[% END %]
more outer
[% CATCH foo %]
caught outer
[% END %]
after
-- expect --
before
outer
inner
caught inner foo
more outer
after

-- test --
before
[% TRY %]
outer
[% TRY %]
inner
[% THROW foo g %]
more inner
[% CATCH foo %]
caught inner foo
[% THROW $error %]
[% CATCH %]
caught inner
[% END %]
more outer
[% CATCH foo %]
caught outer foo [% error.info +%]
[% CATCH %]
caught outer [[% error.type %]] [% error.info +%]
[% END %]
after
-- expect --
before
outer
inner
caught inner foo
caught outer foo golf
after

-- test --
before
[% TRY %]
outer
[% TRY %]
inner
[% THROW foo g %]
more inner
[% CATCH foo %]
caught inner foo
[% THROW bar error.info %]
[% CATCH %]
caught inner
[% END %]
more outer
[% CATCH foo %]
WRONG: caught outer foo [% error.info +%]
[% CATCH bar %]
RIGHT: caught outer bar [% error.info +%]
[% CATCH %]
caught outer [[% error.type %]] [% error.info +%]
[% END %]
after
-- expect --
before
outer
inner
caught inner foo
RIGHT: caught outer bar golf
after

-- test --
before
[% TRY %]
outer
[% TRY %]
inner
[% THROW foo g %]
more inner
[% CATCH foo %]
[% CLEAR %]
caught inner foo
[% THROW bar error.info %]
[% CATCH %]
caught inner
[% END %]
more outer
[% CATCH foo %]
WRONG: caught outer foo [% error.info +%]
[% CATCH bar %]
RIGHT: caught outer bar [% error.info +%]
[% CATCH %]
caught outer [[% error.type %]] [% error.info +%]
[% END %]
after
-- expect --
before
outer
caught inner foo
RIGHT: caught outer bar golf
after

-- test --
before
[% TRY %]
outer
[% TRY %]
inner
[% THROW foo g %]
more inner
[% CATCH foo %]
caught inner foo
[% THROW bar error.info %]
[% CATCH %]
caught inner
[% END %]
more outer
[% CATCH foo %]
WRONG: caught outer foo [% error.info +%]
[% CATCH bar %]
[% CLEAR %]
RIGHT: caught outer bar [% error.info +%]
[% CATCH %]
caught outer [[% error.type %]] [% error.info +%]
[% END %]
after
-- expect --
before
RIGHT: caught outer bar golf
after

-- test --
before
[% TRY %]
outer
[% TRY %]
inner
[% THROW foo g %]
more inner
[% CATCH bar %]
caught inner bar
[% END %]
more outer
[% CATCH foo %]
RIGHT: caught outer foo [% error.info +%]
[% CATCH bar %]
WRONG: caught outer bar [% error.info +%]
[% CATCH %]
caught outer [[% error.type %]] [% error.info +%]
[% END %]
after
-- expect --
before
outer
inner
RIGHT: caught outer foo golf
after


#------------------------------------------------------------------------
# test throwing from Perl code via die()
#------------------------------------------------------------------------
-- test --
[% TRY %]
before
[% throw_egg %]
after
[% CATCH egg %]
caught egg: [% error.info +%]
[% END %]
after
-- expect --
before
caught egg: scrambled
after

-- test --
[% TRY %]
before
[% throw_any %]
after
[% CATCH egg %]
caught egg: [% error.info +%]
[% CATCH %]
caught any: [[% error.type %]] [% error.info %]
[% END %]
after
-- expect --
before
caught any: [undef] undefined error
after

-- test --
[% TRY %]
[% THROW up 'feeling sick' %]
[% CATCH %]
[% error %]
[% END %]
-- expect --
up error - feeling sick

-- test --
[% TRY %]
[% THROW up 'feeling sick' %]
[% CATCH %]
[% e %]
[% END %]
-- expect --
up error - feeling sick

-- test --
[% TRY; THROW food 'cabbage'; CATCH DEFAULT; "caught $e.info"; END %]
-- expect --
caught cabbage


-- test --
[%  TRY; 
	THROW food 'cabbage'; 
     CATCH food; 
	"caught food: $e.info\n";
     CATCH DEFAULT;
	"caught default: $e.info";
     END
 %]
-- expect --
caught food: cabbage

-- test --
[% TRY;
     PROCESS no_such_file;
   CATCH;
     "error: $error\n";
   END;
%]
-- expect --
error: file error - no_such_file: not found

