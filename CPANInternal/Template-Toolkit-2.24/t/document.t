#============================================================= -*-perl-*-
#
# t/document.t
#
# Test the Template::Document module.
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
use Template::Config;
use Template::Document;

$^W = 1;
$Template::Test::DEBUG = 0;
$Template::Document::DEBUG = 0;
#$Template::Parser::DEBUG = 1;
#$Template::Directive::PRETTY = 1;
my $DEBUG = 0;


#------------------------------------------------------------------------
# define a dummy context object for runtime processing
#------------------------------------------------------------------------
package Template::DummyContext;
sub new   { bless { }, shift }
sub visit { }
sub leave { }

package main;

#------------------------------------------------------------------------
# create a document and check accessor methods for blocks and metadata
#------------------------------------------------------------------------
my $doc = Template::Document->new({
    BLOCK     => sub { my $c = shift; return "some output" },
    DEFBLOCKS => {
	foo => sub { return 'the foo block' },
	bar => sub { return 'the bar block' },
    },
    METADATA  => {
	author  => 'Andy Wardley',
	version => 3.14,
    },
});

my $c = Template::DummyContext->new();

ok( $doc );
ok( $doc->author()  eq 'Andy Wardley' );
ok( $doc->version() == 3.14 );
ok( $doc->process($c) eq 'some output' );
ok( ref($doc->block()) eq 'CODE' );
ok( ref($doc->blocks->{ foo }) eq 'CODE' );
ok( ref($doc->blocks->{ bar }) eq 'CODE' );
ok( &{ $doc->block }   eq 'some output' );
ok( &{ $doc->blocks->{ foo } } eq 'the foo block' );
ok( &{ $doc->blocks->{ bar } } eq 'the bar block' );

my $dir   = -d 't' ? 't/test' : 'test';
my $tproc = Template->new({ 
    INCLUDE_PATH => "$dir/src",
});

test_expect(\*DATA, $tproc, { mydoc => $doc });

__END__
-- test --
# test metadata
[% META
   author = 'Tom Smith'
   version = 1.23 
-%]
version [% template.version %] by [% template.author %]
-- expect --
version 1.23 by Tom Smith

# test local block definitions are accessible
-- test --
[% BLOCK foo -%]
   This is block foo
[% INCLUDE bar -%]
   This is the end of block foo
[% END -%]
[% BLOCK bar -%]
   This is block bar
[% END -%]
[% PROCESS foo %]

-- expect --
   This is block foo
   This is block bar
   This is the end of block foo

-- test --
[% META title = 'My Template Title' -%]
[% BLOCK header -%]
title: [% template.title or title %]
[% END -%]
[% INCLUDE header %]
-- expect --
title: My Template Title

-- test --
[% BLOCK header -%]
HEADER
component title: [% component.name %]
 template title: [% template.name %]
[% END -%]
component title: [% component.name %]
 template title: [% template.name %]
[% PROCESS header %]
-- expect --
component title: input text
 template title: input text
HEADER
component title: header
 template title: input text

-- test --
[% META title = 'My Template Title' -%]
[% BLOCK header -%]
title: [% title or template.title  %]
[% END -%]
[% INCLUDE header title = 'A New Title' %]
[% INCLUDE header %]
-- expect --
title: A New Title

title: My Template Title

-- test --
[% INCLUDE $mydoc %]
-- expect --
some output

-- stop --
# test for component.caller and component.callers patch
-- test --
[% INCLUDE one;
   INCLUDE two;
   INCLUDE three;
%]
-- expect --
one, three
two, three
