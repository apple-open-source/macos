#============================================================= -*-perl-*-
#
# t/stop.t
#
# Test the [% STOP %] directive.
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
use lib qw( ./lib ../lib ../blib/lib ../blib/arch ./blib/lib ./blib/arch );
use vars qw( $DEBUG );
use Template::Test;
use Template::Parser;
use Template::Exception;

#$Template::Parser::DEBUG = 1;
$DEBUG = 1;

my $ttblocks = {
    header => sub { "This is the header\n" },
    footer => sub { "This is the footer\n" },
    halt1  => sub { die Template::Exception->new('stop', 'big error') },
};
my $ttvars = {
    halt   => sub { die Template::Exception->new('stop', 'big error') },
};
    
my $ttbare = Template->new(BLOCKS => $ttblocks);
my $ttwrap = Template->new({
    PRE_PROCESS  => 'header',
    POST_PROCESS => 'footer',
    BLOCKS       => $ttblocks,
});
    

test_expect(\*DATA, [ bare => $ttbare, wrapped => $ttwrap ], $ttvars);

__END__

-- test --
This is some text
[% STOP %]
More text
-- expect --
This is some text

-- test --
This is some text
[% halt %]
More text
-- expect --
This is some text

-- test --
This is some text
[% INCLUDE halt1 %]
More text
-- expect --
This is some text

-- test --
This is some text
[% INCLUDE myblock1 %]
More text
[% BLOCK myblock1 -%]
This is myblock1
[% STOP %]
more of myblock1
[% END %]
-- expect --
This is some text
This is myblock1

-- test --
This is some text
[% INCLUDE myblock2 %]
More text
[% BLOCK myblock2 -%]
This is myblock2
[% halt %]
more of myblock2
[% END %]
-- expect --
This is some text
This is myblock2


#------------------------------------------------------------------------
# ensure 'stop' exceptions get ignored by TRY...END blocks
#------------------------------------------------------------------------
-- test --
before
[% TRY -%]
trying
[% STOP -%]
tried
[% CATCH -%]
caught [[% error.type %]] - [% error.info %]
[% END %]
after

-- expect --
before
trying


#------------------------------------------------------------------------
# ensure PRE_PROCESS and POST_PROCESS templates get added with STOP
#------------------------------------------------------------------------

-- test --
-- use wrapped --
This is some text
[% STOP %]
More text
-- expect --
This is the header
This is some text
This is the footer

