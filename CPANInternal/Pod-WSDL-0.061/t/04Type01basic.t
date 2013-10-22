#!/usr/bin/perl -w
package Pod::WSDL::Type;

use Test::More tests => 11;
BEGIN {use_ok('Pod::WSDL::Type')}
use Pod::WSDL::Writer;
use strict;
use warnings;

eval {
	my $a1 = new Pod::WSDL::Type();
};

ok(defined $@, 'new Type dies, if it does not get a name');

my $t1 = new Pod::WSDL::Type(name => 'foo', 
	array => 1, 
	descr => 'a description',
	writer => new Pod::WSDL::Writer);

ok($t1->name eq 'foo', 'Read name argument correctly from input');
ok($t1->array == 1, 'Read array argument correctly from input');
ok($t1->descr eq 'a description', 'Read descr argument correctly from input');
ok((ref $t1->writer eq 'Pod::WSDL::Writer'), 'Read writer argument correctly from input');

$t1->array(0);
ok($t1->array == 0, 'Setting array member works');

package Main;
use Test::More;

my $t2 = new Pod::WSDL::Type(name => 'foo', 
	array => 1, 
	descr => 'a description',
	writer => new Pod::WSDL::Writer);

eval {$t2->writer;};
ok(defined $@, 'Type does not allow getting of writer');

eval {$t2->name('bar');};
ok(defined $@, 'Type does not allow setting of name');

eval {$t2->descr('blah');};
ok(defined $@, 'Type does not allow setting of descr');

eval {$t2->writer(new Pod::WSDL::Writer);};
ok(defined $@, 'Type does not allow setting of writer');
