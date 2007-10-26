#!/usr/bin/perl -w
package Foo;
use Pod::WSDL::AUTOLOAD;

our @ISA = qw/Pod::WSDL::AUTOLOAD/;

sub new {
	my $pgk = shift;
	bless {
		_bar => 'blah',
	}, $pgk
}

1;

package main;
use Test::More tests => 3;

$foo = Foo->new;
ok($foo->bar eq 'blah', '"_bar" retrievable with "bar".');
$foo->bar('bloerch');  # sets _bar to 'bloerch'
ok($foo->bar eq 'bloerch', '"_bar" settable with "bar".');

eval {
	$foo->boerk;
};

ok($@, 'Using method not equivalent to any attribute croaks');



