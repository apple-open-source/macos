#!/usr/bin/perl -w
use Test::More tests => 15;
BEGIN {use_ok('Pod::WSDL::Param')}
use strict;
use warnings;

eval {
	my $a1 = new Pod::WSDL::Param();
};

ok(defined $@, 'new dies, if it does not get a string');

eval {
	my $a1 = new Pod::WSDL::Param('myParam $string blah blah ...');
};

ok(defined $@, 'new dies, if it does not get a string beginning with _IN, _OUT, or _INOUT');

eval {
	my $a1 = new Pod::WSDL::Param('_IN myParam string blah blah ...');
};

ok(defined $@, 'new dies, if array/scalar type is not specified');

my $a1 = new Pod::WSDL::Param('_IN myParam $string blah blah ...');

ok($a1->name eq 'myParam', 'Read name correctly from input');
ok($a1->type eq 'string', 'Read type correctly from input');
ok($a1->paramType eq 'IN', 'Read in type correctly from input');
ok($a1->array == 0, 'Read scalar type correctly from input');
ok($a1->descr eq 'blah blah ...', 'Read descr correctly from input');

$a1 = new Pod::WSDL::Param('   _IN myParam $string blah blah ...');
ok($a1->name eq 'myParam', 'Handles whitespace before _IN correctly.');

$a1 = new Pod::WSDL::Param('_IN myParam @string blah blah ...');
ok($a1->array == 1, 'Read array type correctly from input');

$a1 = new Pod::WSDL::Param('_IN myParam @string');
ok($a1->descr eq '', 'No description is handled correctly');

$a1 = new Pod::WSDL::Param('_OUT myParam @string');
ok($a1->paramType eq 'OUT', 'Read in type correctly from input');

$a1 = new Pod::WSDL::Param('_INOUT myParam @string');
ok($a1->paramType eq 'INOUT', 'Read inout type correctly from input');

eval {
	$a1->name('foo');
};

{
	no warnings;
	ok($@ == undef, 'Renaming param is forbidden.');
}
