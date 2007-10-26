#!/usr/bin/perl -w
use Test::More tests => 7;
BEGIN {use_ok('Pod::WSDL::Doc')}
use strict;
use warnings;

eval {
	my $a1 = new Pod::WSDL::Doc();
};

ok(defined $@, 'new dies, if it does not get a string');

eval {
	my $a1 = new Pod::WSDL::Doc('blah blah ...');
};

ok(defined $@, 'new dies, if it does not get a string beginning with _DOC');

my $a1 = new Pod::WSDL::Doc('_DOC blah blah ...');

ok($a1->descr eq 'blah blah ...', 'Read descr correctly from input');

$a1 = new Pod::WSDL::Doc('   _DOC blah blah ...');
ok($a1->descr eq 'blah blah ...', 'Handles whitespace before _DOC correctly.');

$a1 = new Pod::WSDL::Doc('_DOC');
ok($a1->descr eq '', 'No description is handled correctly');

$a1->descr('more blah');
ok($a1->descr eq 'more blah', 'Setting description works');
