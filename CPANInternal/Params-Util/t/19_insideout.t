#!/usr/bin/perl

# Test for a custom isa method that returns the same way that
# Object::InsideOut does.

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
	$ENV{PERL_PARAMS_UTIL_PP} ||= 1;
}

use Test::More tests => 2;
use Scalar::Util ();
use Params::Util ();





#####################################################################
# Create an object and test it

SCOPE: {
	my $object = Foo->new;
	ok( Scalar::Util::blessed($object), 'Foo' );
	my $instance = Params::Util::_INSTANCE($object, 'Foo');
	is( $instance, undef, '_INSTANCE correctly returns undef' );
}





#####################################################################
# Create a package to simulate Object::InsideOut

CLASS: {
	package Foo;

	sub new {
		my $foo  = 1234;
		my $self = \$foo;
		bless $self, $_[0];
		return $self;
	}

	sub isa {
		return ('');
	}

	1;
}
