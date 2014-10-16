#============================================================= -*-perl-*-
#
# t/factory.t
#
# Test use of a modified directive factory, based on something that
# pudge suggested on #perl.
#
# Written by Andy Wardley <abw@kfs.org>
#
# Copyright (C) 1996-2001 Andy Wardley.  All Rights Reserved.
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

# uncomment these lines to see how generate Perl code 
# for constant.* is expanded at parse time
#Template::Parser::DEBUG = 1;
#Template::Directive::PRETTY = 1;

package My::Directive;
use base qw( Template::Directive );

my $constants = {
    pi => 3.14,
    e  => 2.718,
};

sub ident {
    my ($class, $ident) = @_;

    # note single quoting of 'constant'
    if (ref $ident eq 'ARRAY' && $ident->[0] eq "'constant'") {
	my $key = $ident->[2];
	$key =~ s/'//g;
	return $constants->{ $key } || '';
    }
    return $class->SUPER::ident($ident);
}

package main;

my $cfg = { 
    FACTORY => 'My::Directive',
};

my $vars = {
    foo => {
	bar => 'Place to purchase drinks',
	baz => 'Short form of "Basil"',
    },
};

test_expect(\*DATA, $cfg, $vars);

__DATA__
-- test --
[% foo.bar %]
-- expect --
Place to purchase drinks

-- test --
[% constant.pi %]
-- expect --
3.14
