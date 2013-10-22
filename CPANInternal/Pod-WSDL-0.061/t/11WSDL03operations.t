#!/usr/bin/perl -w
package Pod::WSDL;
use Test::More tests => 23;
BEGIN {use_ok('Pod::WSDL');}
use lib length $0 > 14 ? substr $0, 0, length($0) - 20 : '.';
use strict;
use warnings;
use XML::XPath;

my $p = new Pod::WSDL(source => 'My::OperationTest',
	               location => 'http://localhost/My/OperationTest',
	               pretty => 1,
	               withDocumentation => 1);

my $xmlOutput = $p->WSDL;
my $xp = XML::XPath->new(xml => $xmlOutput);

# test general structure
ok($xp->exists('/wsdl:definitions/wsdl:message[@name = "testGeneralRequest"]'), 'Found message element "testGeneralRequest" in xml output.');
ok($xp->exists('/wsdl:definitions/wsdl:message[@name = "testGeneralRequest"]/wsdl:part[@name = "in" and @type = "xsd:string"]'), 'Found part element "in" for message "testGeneralRequest" in xml output.');

ok($xp->exists('/wsdl:definitions/wsdl:message[@name = "testGeneralResponse"]'), 'Found message element "testGeneralResponse" in xml output.');
ok($xp->exists('/wsdl:definitions/wsdl:message[@name = "testGeneralResponse"]/wsdl:part[@name = "testGeneralReturn" and @type = "xsd:string"]'), 'Found part element "testGeneralReturn" for message "testGeneralResponse" in xml output.');

ok($xp->exists('/wsdl:definitions/wsdl:portType[@name = "MyOperationTestHandler"]'), 'Found portType element "MyOperationTestHandler" in xml output.');
ok($xp->exists('/wsdl:definitions/wsdl:portType[@name = "MyOperationTestHandler"]/wsdl:operation[@name = "testGeneral" and @parameterOrder = "in"]'), 'Found operation element "testGeneral" in portType in xml output.');
ok($xp->exists('/wsdl:definitions/wsdl:portType[@name = "MyOperationTestHandler"]/wsdl:operation[@name = "testGeneral" and @parameterOrder = "in"]/wsdl:documentation[text() = "bla bla"]'), 'Found documentation for operation element "testGeneral".');
ok($xp->exists('/wsdl:definitions/wsdl:portType[@name = "MyOperationTestHandler"]/wsdl:operation[@name = "testGeneral" and @parameterOrder = "in"]/wsdl:input[@message = "impl:testGeneralRequest" and @name="testGeneralRequest"]'), 'Found input message for operation element "testGeneral".');
ok($xp->exists('/wsdl:definitions/wsdl:portType[@name = "MyOperationTestHandler"]/wsdl:operation[@name = "testGeneral" and @parameterOrder = "in"]/wsdl:output[@message = "impl:testGeneralResponse" and @name="testGeneralResponse"]'), 'Found output message for operation element "testGeneral".');

# test parameters: _IN, _OUT, _INOUT
ok($xp->exists('/wsdl:definitions/wsdl:message[@name = "testInOutRequest"]/wsdl:part[@name = "in" and @type = "xsd:string"]'), 'Found part element "in" in xml output.');
ok($xp->exists('/wsdl:definitions/wsdl:message[@name = "testInOutRequest"]/wsdl:part[@name = "out" and @type = "xsd:string"]'), 'Found part element "out" in xml output.');
ok($xp->exists('/wsdl:definitions/wsdl:message[@name = "testInOutRequest"]/wsdl:part[@name = "inout" and @type = "xsd:string"]'), 'Found part element "inout" in xml output.');

# test faults
ok($xp->exists('/wsdl:definitions/wsdl:message[@name = "MyFoo"]'), 'Found message element "MyFoo" in xml output.');
ok($xp->exists('/wsdl:definitions/wsdl:message[@name = "MyFoo"]/wsdl:part[@name = "fault" and @type = "tns1:MyFoo"]'), 'Found part element "fault" for message "MyFoo" in xml output.');
ok($xp->exists('/wsdl:definitions/wsdl:portType[@name = "MyOperationTestHandler"]/wsdl:operation[@name = "testGeneral" and @parameterOrder = "in"]/wsdl:fault[@message = "impl:MyFoo" and @name="MyFoo"]'), 'Found fault message for operation element "testGeneral".');

# test arrays
ok($xp->exists('/wsdl:definitions/wsdl:message[@name = "testArrayRequest"]/wsdl:part[@name = "in" and @type = "tns1:ArrayOfString"]'), 'Found correct part element "in" for message "testArrayRequest" in xml output.');
ok($xp->exists('/wsdl:definitions/wsdl:message[@name = "testArrayResponse"]/wsdl:part[@name = "testArrayReturn" and @type = "tns1:ArrayOfString"]'), 'Found correct part element "testArrayReturn" for message "testArrayResponse" in xml output.');

# test empty message
ok($xp->exists('/wsdl:definitions/wsdl:message[@name = "testOnewayRequest"]'), 'Found message element with name "testOnewayRequest" for message "testOneway" in xml output.');

# test oneway message
ok($xp->exists('/wsdl:definitions/wsdl:message[@name = "empty"]'), 'Found message element with name "empty" for message "testEmpty" in xml output.');
ok($xp->exists('/wsdl:definitions/wsdl:portType[@name = "MyOperationTestHandler"]/wsdl:operation[@name = "testOneway" and @parameterOrder = "in"]/wsdl:input[@message = "impl:testOnewayRequest" and @name="testOnewayRequest"]'), 'Found input message for operation element "testOneway".');
ok(!$xp->exists('/wsdl:definitions/wsdl:portType[@name = "MyOperationTestHandler"]/wsdl:operation[@name = "testOneway" and @parameterOrder = "in"]/wsdl:output'), 'Did not find output message (which is correct) for operation element "testOneway".');


# test method without wsdl pod
ok(!$xp->exists('/wsdl:definitions/wsdl:message[@name = "testWithoutPodRequest"]') && !$xp->exists('/wsdl:definitions/wsdl:message[@name = "testWithoutPodResponse"]'), 'Non pod messages not found xml output.');

print $xmlOutput;