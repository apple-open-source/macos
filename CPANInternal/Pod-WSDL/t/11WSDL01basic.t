#!/usr/bin/perl -w
package Pod::WSDL;
use Test::More tests => 33;
BEGIN {use_ok('Pod::WSDL');}
use lib length $0 > 10 ? substr $0, 0, length($0) - 16 : '.';
use strict;
use warnings;
use XML::XPath;

eval {
	new Pod::WSDL(source => 'bla');
};

ok($@ =~ /I need a location/, 'new dies, if it does not get a location');

eval {
	new Pod::WSDL(location => 'bla');
};

ok($@ =~ /I need a file or module name or a filehandle, died/, 'new dies, if it does not get a source');

my $p = new Pod::WSDL(source => 'My::Server', 
	location => 'http://localhost/My/Server',
	pretty => 1,
	withDocumentation => 1);

ok($p->writer->{_pretty}, 'Received pretty argument correctly');
ok($p->writer->{_withDocumentation}, 'Received withDocumentation argument correctly');
ok($p->location eq 'http://localhost/My/Server', 'Received location argument correctly');
ok($p->{_source} eq 'My::Server', 'Received source argument correctly');
ok($p->{_baseName} eq 'MyServer', 'Generated base name argument correctly');

$p->location('http://localhost/My/Other/Server');
ok($p->location eq 'http://localhost/My/Other/Server', 'Setting location works');

ok($p->namespaces->{'xmlns:impl'} eq 'http://localhost/My/Server', 'Generated xmlns:impl namespace correctly');
ok($p->namespaces->{'xmlns:wsdlsoap'} eq 'http://schemas.xmlsoap.org/wsdl/soap/', 'Generated xmlns:soap namespace correctly');
ok($p->namespaces->{'xmlns:wsdl'} eq 'http://schemas.xmlsoap.org/wsdl/', 'Generated xmlns:wsdl namespace correctly');
ok($p->namespaces->{'xmlns:soapenc'} eq 'http://schemas.xmlsoap.org/soap/encoding/', 'Generated xmlns:soapenc namespace correctly');
ok($p->namespaces->{'xmlns:xsd'} eq 'http://www.w3.org/2001/XMLSchema', 'Generated xmlns:xsd namespace correctly');
ok($p->namespaces->{'xmlns:tns1'} eq 'http://localhost/My/Server', 'Generated xmlns:tns1 namespace correctly');

ok(ref $p->writer->{_outStr} eq 'XML::Writer::_String', 'Initialized outStr for writer correctly.');
ok(ref $p->writer->{_writer} eq 'XML::Writer', 'Found an XML::Writer for output');
ok(ref $p->generateNS eq 'CODE', 'Initialized generateNS correctly');
ok($p->writer->{_indent} == 1, 'Initialized indentation correctly');
ok($p->writer->{_lastTag} eq '', 'Initialized lastTag correctly');

my $loc = $p->location;
ok($p->WSDL =~ m#<!-- WSDL for $loc created by Pod::WSDL version: $Pod::WSDL::VERSION on .*? -->#, 'Generated comment correctly');

# arguments of method WSDL()
$p = new Pod::WSDL(source => 'My::OperationTest', 
	location => 'http://localhost/My/OperationTest',
	pretty => 1,
	withDocumentation => 1);
my $xp = XML::XPath->new(xml => $p->WSDL);
ok($xp->exists('/wsdl:definitions/wsdl:types/schema/complexType[@name="MyFoo"]/annotation/documentation'), 'Found documentation in schema part (complexType).');
ok($xp->exists('/wsdl:definitions/wsdl:types/schema/complexType[@name="MyFoo"]/sequence/element[@name="_bar"]/annotation/documentation'), 'Found documentation in schema part (element).');
ok($xp->exists('/wsdl:definitions/wsdl:portType[@name="MyOperationTestHandler"]/wsdl:operation[@name="testGeneral"]/wsdl:documentation'), 'Found documentation in operation part.');

#print $p->WSDL;

$xp = XML::XPath->new(xml => $p->WSDL(withDocumentation => 0));
ok(!$xp->exists('/wsdl:definitions/wsdl:types/schema/complexType[@name="MyFoo"]/annotation/documentation'), 'Switched off documentation -> did not find documentation in schema part (complexType).');
ok(!$xp->exists('/wsdl:definitions/wsdl:types/schema/complexType[@name="MyFoo"]/sequence/element[@name="_bar"]/annotation/documentation'), 'Switched off documentation -> did not find documentation in schema part (element).');
ok(!$xp->exists('/wsdl:definitions/wsdl:portType[@name="MyOperationTestHandler"]/wsdl:operation[@name="testGeneral"]/wsdl:documentation'), 'Switched off documentation -> did not find documentation in operation part.');

$xp = XML::XPath->new(xml => $p->WSDL(withDocumentation => 1));
ok($xp->exists('/wsdl:definitions/wsdl:types/schema/complexType[@name="MyFoo"]/annotation/documentation'), 'Found documentation in schema part (complexType).');
ok($xp->exists('/wsdl:definitions/wsdl:types/schema/complexType[@name="MyFoo"]/sequence/element[@name="_bar"]/annotation/documentation'), 'Found documentation in schema part (element).');
ok($xp->exists('/wsdl:definitions/wsdl:portType[@name="MyOperationTestHandler"]/wsdl:operation[@name="testGeneral"]/wsdl:documentation'), 'Found documentation in operation part.');

$p = new Pod::WSDL(source => 'My::Server', 
	location => 'http://localhost/My/Server',
	pretty => 1);

my $outputtestFile = $0;
$outputtestFile =~ s![^/]+$!outputtest001.xml!;

my $outputtest;

{
	$/ = undef;
	open TEST, "$outputtestFile" or die "Could not open $outputtestFile";
	$outputtest = <TEST>;
	close TEST;
}


my $tmp = $p->WSDL;
$tmp =~ s/<!-- WSDL.*?-->\n//;
ok($outputtest eq $tmp, "Pretty works.");

$outputtestFile = $0;
$outputtestFile =~ s![^/]+$!outputtest002.xml!;

my $outputtest2;

{
	$/ = undef;
	open TEST, "$outputtestFile" or die "Could not open $outputtestFile";
	$outputtest2 = <TEST>;
	close TEST;
}


$tmp = $p->WSDL(pretty => 0);
$tmp =~ s/<!-- WSDL.*?-->\n//;
#print "--->$tmp<---\n";
ok($outputtest2 eq $tmp, "Switch pretty off works.");

$tmp = $p->WSDL(pretty => 1);
$tmp =~ s/<!-- WSDL.*?-->\n//;
ok($outputtest eq $tmp, "Switch pretty on works.");
