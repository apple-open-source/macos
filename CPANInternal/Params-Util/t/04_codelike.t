#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
	$ENV{PERL_PARAMS_UTIL_PP} ||= 0;
}

sub _CODELIKE($);

use Test::More;
use File::Spec::Functions ':ALL';
use Scalar::Util qw(
	blessed
	reftype
	refaddr
);
use overload;

sub c_ok { is(
	refaddr(_CODELIKE($_[0])),
	refaddr($_[0]),
	"callable: $_[1]",
) }

sub nc_ok {
	my $left = shift;
	$left = _CODELIKE($left);
	is( $left, undef, "not callable: $_[0]" );
}

my @callables = (
	"callable itself"                         => \&_CODELIKE,
	"a boring plain code ref"                 => sub {},
	'an object with overloaded &{}'           => C::O->new,
	'a object build from a coderef'           => C::C->new,
	'an object with inherited overloaded &{}' => C::O::S->new, 
	'a coderef blessed into CODE'             => (bless sub {} => 'CODE'),
);

my @uncallables = (
	"undef"                                   => undef,
	"a string"                                => "a string",
	"a number"                                => 19780720,
	"a ref to a ref to code"                  => \(sub {}),
	"a boring plain hash ref"                 => {},
	'a class that builds from coderefs'       => "C::C",
	'a class with overloaded &{}'             => "C::O",
	'a class with inherited overloaded &{}'   => "C::O::S",
	'a plain boring hash-based object'        => UC->new,
	'a non-coderef blessed into CODE'         => (bless {} => 'CODE'),
);

my $tests = (@callables + @uncallables) / 2 + 2;

if ( $] > 5.006 ) {
	push @uncallables, 'a regular expression', qr/foo/;
	$tests += 1;
}

plan tests => $tests;

# Import the function
use_ok( 'Params::Util', '_CODELIKE' );
ok( defined *_CODELIKE{CODE}, '_CODELIKE imported ok' );

while ( @callables ) {
	my $name   = shift @callables;
	my $object = shift @callables;
	c_ok( $object, $name );
}

while ( @uncallables ) {
	my $name   = shift @uncallables;
	my $object = shift @uncallables;
	nc_ok( $object, $name );
}





######################################################################
# callable: is a blessed code ref

package C::C;

sub new {
	bless sub {} => shift;
}





######################################################################
# callable: overloads &{}
# but only objects are callable, not class

package C::O;

sub new {
	bless {} => shift;
}
use overload '&{}'  => sub { sub {} };
use overload 'bool' => sub () { 1 };





######################################################################
# callable: subclasses C::O

package C::O::S;

use vars qw{@ISA};
BEGIN {
	@ISA = 'C::O';
}





######################################################################
# uncallable: some boring object with no codey magic

package UC;

sub new {
	bless {} => shift;
}
