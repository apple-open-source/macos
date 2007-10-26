#!/usr/bin/perl -w
package Foo;
use Pod::WSDL::AUTOLOAD;

our @ISA = qw/Pod::WSDL::AUTOLOAD/;

our %FORBIDDEN_METHODS = (
	bar     => {get => 0, set => 0},
	boerk   => {get => 0, set => 1},
	bloerch => {get => 1, set => 0},
	boerps  => {get => 1, set => 1},
);

sub new {
	my $pgk = shift;
	bless {
		_bar => 'blah',
	}, $pgk
}

sub miaow {
	my $me = shift;
	$me->bar;
	$me->bar('br');
}

1;

package main;
use Test::More tests => 9;
no warnings;

my $foo = Foo->new;

eval {$foo->bar;};
ok($@, 'Both forbidden: Using getter croaks');

eval {$foo->bar('br');};
ok($@, 'Both forbidden: Using setter croaks');

eval {$foo->boerk;};
ok($@ == undef, 'Setter forbidden: Using getter does not croak');

eval {$foo->boerk('br');};
ok($@, 'Setter forbidden: Using setter croaks');

eval {$foo->bloerch;};
ok($@, 'Getter forbidden: Using getter croaks');

eval {$foo->bloerch('br');};
ok($@ == undef, 'Getter forbidden: Using setter does not croak');

eval {$foo->boeps;};
ok($@ == undef, 'Nothing forbidden: Using getter does not croak');

eval {$foo->boerps('br');};
ok($@ == undef, 'Nothing forbidden: Using setter does not croak');

eval{$foo->miaow};
ok($@ == undef, 'Calling accessors from within package does not croak');

