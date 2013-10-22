#!/usr/bin/perl -w
use Test::More tests => 5;
BEGIN {use_ok('Pod::WSDL::Utils')}
use strict;
use warnings;

# test getTypeDescr()
ok(Pod::WSDL::Utils::getTypeDescr('int', 0, undef) eq 'xsd:int', 'getTypeDescr() returns simple scalar type correctly');
ok(Pod::WSDL::Utils::getTypeDescr('int', 1, undef) eq 'tns1:ArrayOfInt', 'getTypeDescr() returns simple array type correctly');
ok(Pod::WSDL::Utils::getTypeDescr('foo', 0, OwnType->new) eq 'tns1:wsdlName', 'getTypeDescr() returns complex scalar type correctly');
ok(Pod::WSDL::Utils::getTypeDescr('foo', 1, OwnType->new) eq 'tns1:ArrayOfWsdlName', 'getTypeDescr() returns complex array type correctly');

package OwnType;

sub new {
	bless {}, 'OwnType';
}

sub wsdlName {
	return 'wsdlName';
}