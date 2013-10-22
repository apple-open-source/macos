#!/usr/bin/perl -w
package Pod::WSDL::Method;

use Test::More tests => 21;
BEGIN {use_ok('Pod::WSDL::Method')}
use strict;
use warnings;
use Pod::WSDL::Return;
use Pod::WSDL::Param;
use Pod::WSDL::Doc;
use Pod::WSDL::Fault;
use Pod::WSDL::Writer;

my $m;

eval {$m = new Pod::WSDL::Method(writer => 1);};
ok($@ =~ /a method needs a name/i, 'new dies, if it does not get a name');

eval {$m = new Pod::WSDL::Method(name => 'bla');};
ok($@ =~ /a method needs a writer/i, 'new dies, if it does not get a writer');

my $ret1 = new Pod::WSDL::Return('_RETURN $string This returns blah ...');
my $ret2 = new Pod::WSDL::Return('_RETURN $boolean This returns blah blah ...');
my $doc1 = new Pod::WSDL::Doc('_DOC This method is for blah ...');
my $doc2 = new Pod::WSDL::Doc('_DOC This method is for blah blah ...');
my $par1 = new Pod::WSDL::Param('_IN myParam $string This parameter is for blah ...');
my $par2 = new Pod::WSDL::Param('_OUT yourParam $string This parameter is for blah ...');
my $fau1 = new Pod::WSDL::Fault('_FAULT My::Fault This happens if something happens');
my $fau2 = new Pod::WSDL::Fault('_FAULT My::Fault This happens if nothing happens');

$m = new Pod::WSDL::Method(name => "myMethod", return => $ret1, doc => $doc1, params => [$par1], faults => [$fau1], writer => new Pod::WSDL::Writer);

ok($m->name eq 'myMethod', 'Retrieving name works');
ok($m->oneway == 0, 'Default for oneway set correctly');
ok((ref $m->return eq 'Pod::WSDL::Return' and $m->return->type eq 'string'), 'Retrieving return works');
ok((ref $m->doc eq 'Pod::WSDL::Doc' and $m->doc->descr eq 'This method is for blah ...'), 'Retrieving doc works');
ok((ref $m->params->[0] eq 'Pod::WSDL::Param' and $m->params->[0]->name eq 'myParam'), 'Retrieving param works');
ok((ref $m->faults->[0] eq 'Pod::WSDL::Fault' and $m->faults->[0]->type eq 'My::Fault'), 'Retrieving fault works');
ok(ref $m->writer eq 'Pod::WSDL::Writer', 'Pod::WSDL::Writer correctly initialized');

$m->return($ret2);
$m->doc($doc2);

ok((ref $m->return eq 'Pod::WSDL::Return' and $m->return->type eq 'boolean'), 'Setting return works');
ok((ref $m->doc eq 'Pod::WSDL::Doc' and $m->doc->descr eq 'This method is for blah blah ...'), 'Setting doc works');

$m->addParam($par2);
$m->addFault($fau2);

ok((ref $m->params->[0] eq 'Pod::WSDL::Param' and $m->params->[0]->name eq 'myParam'), 'Adding param does not influence existing params');
ok((ref $m->faults->[0] eq 'Pod::WSDL::Fault' and $m->faults->[0]->type eq 'My::Fault'), 'Adding fault does not influence existing faults');
ok((ref $m->params->[1] eq 'Pod::WSDL::Param' and $m->params->[1]->name eq 'yourParam'), 'Setting param works');
ok((ref $m->faults->[1] eq 'Pod::WSDL::Fault' and $m->faults->[1]->descr eq 'This happens if nothing happens'), 'Setting fault works');

$m->oneway(1);
ok($m->oneway == 1, 'Setting param oneway works');

$m->oneway(0);
ok($m->oneway == 0, 'Unsetting param oneway works');

eval {
	$m->name('foo');
};

{
	no warnings;
	ok($@ == undef, 'Renaming method is forbidden.');
}

ok($m->requestName eq 'fooRequest', 'Method requestName() works');
ok($m->responseName eq 'fooResponse', 'Method responseName() works');
