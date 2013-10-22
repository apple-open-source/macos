#============================================================= -*-perl-*-
#
# t/args.t
#
# Testing the passing of positional and named arguments to sub-routine and 
# object methods.
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
use Template::Constants qw( :status );
$^W = 1;

#------------------------------------------------------------------------
# define simple object and package sub for reporting arguments passed
#------------------------------------------------------------------------

package MyObj;
use base qw( Template::Base );

sub foo {
    my $self = shift;
    return "object:\n" . args(@_);
}

sub args {
    my @args = @_;
    my $named = ref $args[$#args] eq 'HASH' ? pop @args : { };
    local $" = ', ';
    
    return "  ARGS: [ @args ]\n NAMED: { "
	. join(', ', map { "$_ => $named->{ $_ }" } sort keys %$named)
	. " }\n";
}


#------------------------------------------------------------------------
# main tests
#------------------------------------------------------------------------

package main;

use Template::Parser;
$Template::Test::DEBUG = 0;
$Template::Parser::DEBUG = 0;

my $replace = callsign();
$replace->{ args } = \&MyObj::args;
$replace->{ obj  } = MyObj->new();

test_expect(\*DATA, { INTERPOLATE => 1 }, $replace);


__DATA__
-- test --
[% args(a b c) %]
-- expect --
  ARGS: [ alpha, bravo, charlie ]
 NAMED: {  }

-- test --
[% args(a b c d=e f=g) %]
-- expect --
  ARGS: [ alpha, bravo, charlie ]
 NAMED: { d => echo, f => golf }

-- test --
[% args(a, b, c, d=e, f=g) %]
-- expect --
  ARGS: [ alpha, bravo, charlie ]
 NAMED: { d => echo, f => golf }

-- test --
[% args(a, b, c, d=e, f=g,) %]
-- expect --
  ARGS: [ alpha, bravo, charlie ]
 NAMED: { d => echo, f => golf }

-- test --
[% args(d=e, a, b, f=g, c) %]
-- expect --
  ARGS: [ alpha, bravo, charlie ]
 NAMED: { d => echo, f => golf }

-- test --
[% obj.foo(d=e, a, b, f=g, c) %]
-- expect --
object:
  ARGS: [ alpha, bravo, charlie ]
 NAMED: { d => echo, f => golf }

-- test --
[% obj.foo(d=e, a, b, f=g, c).split("\n").1 %]
-- expect --
  ARGS: [ alpha, bravo, charlie ]

