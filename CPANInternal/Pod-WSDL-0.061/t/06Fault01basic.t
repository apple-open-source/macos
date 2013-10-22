#!/usr/bin/perl -w
use Test::More tests => 10;
BEGIN {use_ok('Pod::WSDL::Fault')}
use strict;
use warnings;

eval {
	my $a1 = new Pod::WSDL::Fault();
};

ok(defined $@, 'new dies, if it does not get a string');

eval {
	my $a1 = new Pod::WSDL::Fault('blah blah ...');
};

ok(defined $@, 'new dies, if it does not get a string beginning with _FAULT');

eval {
	my $a1 = new Pod::WSDL::Fault('_FAULT My::Fault blah blah ...');
};

ok(defined $@, 'new dies, if array/scalar type is not specified');

my $a1 = new Pod::WSDL::Fault('_FAULT My::Fault blah blah ...');

ok($a1->wsdlName eq 'MyFault', 'Read wsdl name correctly from input');
ok($a1->type eq 'My::Fault', 'Read type correctly from input');
ok($a1->descr eq 'blah blah ...', 'Read descr correctly from input');

$a1 = new Pod::WSDL::Fault('   _FAULT My::Fault blah blah ...');
ok($a1->wsdlName eq 'MyFault', 'Handles whitespace before _FAULT correctly.');

$a1 = new Pod::WSDL::Fault('_FAULT My::Fault');
ok($a1->descr eq '', 'No description is handled correctly');

eval {
	$a1->type('foo');
};

{
	no warnings;
	ok($@ == undef, 'Renaming fault is forbidden.');
}
