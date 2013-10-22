#============================================================= -*-perl-*-
#
# t/stashc.t
#
# Template script testing the Template::Stash::Context module.
# Currently only partially complete.
#
# Written by Andy Wardley <abw@kfs.org>
#
# Copyright (C) 1996-2001 Andy Wardley.  All Rights Reserved.
# Copyright (C) 1998-2001 Canon Research Centre Europe Ltd.
#
# This is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
# $Id$
#
#========================================================================

use strict;
use lib qw( ../lib );
use Template::Constants qw( :status );
use Template::Stash::Context;
use Template::Test;
$^W = 1;

my $count = 20;
my $data = {
    foo => 10,
    bar => {
	baz => 20,
    },
    baz => sub {
	return {
	    boz => ($count += 10),
	    biz => (shift || '<undef>'),
	};
    },
    numbers => sub {
	return wantarray ? (1, 2, 3) : "one two three";
    }
};

my $stash = Template::Stash::Context->new($data);

match( $stash->get('foo'), 10 );
match( $stash->get([ 'bar', 0, 'baz', 0 ]), 20 );
match( $stash->get('bar.baz'), 20 );
match( $stash->get('bar(10).baz'), 20 );
match( $stash->get('baz.boz'), 30 );
match( $stash->get('baz.boz'), 40 );
match( $stash->get('baz.biz'), '<undef>' );
match( $stash->get('baz(50).biz'), '<undef>' );   # args are ignored

$stash->set( 'bar.buz' => 100 );
match( $stash->get('bar.buz'), 100 );

test_expect(\*DATA, { STASH => $stash });

__DATA__
-- test --
[% numbers.join(', ') %]
-- expect --
1, 2, 3

-- test --
[% numbers.scalar %]
-- expect --
one two three

-- test --
[% numbers.ref %]
-- expect --
CODE


