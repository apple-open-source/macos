#!/usr/bin/perl -w
package Pod::WSDL;
use Test::More tests => 15;
BEGIN {use_ok('Pod::WSDL');}
use lib length $0 > 11 ? substr $0, 0, length($0) - 17 : '.';
use strict;
use warnings;
use XML::XPath;

my $p = new Pod::WSDL(source => 'My::BindingTest',
	               location => 'http://localhost/My/Test',
	               pretty => 1,
	               withDocumentation => 1);

my $xmlOutput = $p->WSDL;
my $xp = XML::XPath->new(xml => $xmlOutput);

#print $xmlOutput;
#print XML::XPath::XMLParser::as_string(($xp->find('/wsdl:definitions/wsdl:binding')->get_nodelist())[0]);

# test general structure
# binding
ok($xp->exists('/wsdl:definitions/wsdl:binding[@name="MyBindingTestSoapBinding" and @type="impl:MyBindingTestHandler"]'), 'Found wsdl:binding element in xml output.');

# wsdlsoap:binding
ok($xp->exists('/wsdl:definitions/wsdl:binding[@name="MyBindingTestSoapBinding" and @type="impl:MyBindingTestHandler"]/wsdlsoap:binding[@style="rpc" and @transport="http://schemas.xmlsoap.org/soap/http"]'), 'Found wsdlsoap:binding element in wsdl:binding.');

# operation
ok($xp->exists('/wsdl:definitions/wsdl:binding[@name="MyBindingTestSoapBinding" and @type="impl:MyBindingTestHandler"]/wsdl:operation[@name="testGeneral"]'), 'Found operation "testGeneral" element in wsdl:binding.');

# wsdlsoap:operation
ok($xp->exists('/wsdl:definitions/wsdl:binding[@name="MyBindingTestSoapBinding" and @type="impl:MyBindingTestHandler"]/wsdl:operation[@name="testGeneral"]/wsdlsoap:operation[@soapAction = ""]'), 'Found wsdlsoap:operation in operation "testGeneral" element.');

# input
ok($xp->exists('/wsdl:definitions/wsdl:binding[@name="MyBindingTestSoapBinding" and @type="impl:MyBindingTestHandler"]/wsdl:operation[@name="testGeneral"]/wsdl:input[@name = "testGeneralRequest"]'), 'Found wsdl:input in operation "testGeneral" element.');
ok($xp->exists('/wsdl:definitions/wsdl:binding[@name="MyBindingTestSoapBinding" and @type="impl:MyBindingTestHandler"]/wsdl:operation[@name="testGeneral"]/wsdl:input[@name = "testGeneralRequest"]/wsdlsoap:body[@encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" and @namespace="http://localhost/My/BindingTest" and @use="encoded"]'), 'Found wsdlsoap:body in wsdl:input element.');

# output
ok($xp->exists('/wsdl:definitions/wsdl:binding[@name="MyBindingTestSoapBinding" and @type="impl:MyBindingTestHandler"]/wsdl:operation[@name="testGeneral"]/wsdl:output[@name = "testGeneralResponse"]'), 'Found wsdl:output in operation "testGeneral" element.');
ok($xp->exists('/wsdl:definitions/wsdl:binding[@name="MyBindingTestSoapBinding" and @type="impl:MyBindingTestHandler"]/wsdl:operation[@name="testGeneral"]/wsdl:output[@name = "testGeneralResponse"]/wsdlsoap:body[@encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" and @namespace="http://localhost/My/BindingTest" and @use="encoded"]'), 'Found wsdlsoap:body in wsdl:output element.');

# fault
ok($xp->exists('/wsdl:definitions/wsdl:binding[@name="MyBindingTestSoapBinding" and @type="impl:MyBindingTestHandler"]/wsdl:operation[@name="testGeneral"]/wsdl:fault[@name = "MyFoo"]'), 'Found wsdl:fault in operation "testGeneral" element.');
ok($xp->exists('/wsdl:definitions/wsdl:binding[@name="MyBindingTestSoapBinding" and @type="impl:MyBindingTestHandler"]/wsdl:operation[@name="testGeneral"]/wsdl:fault[@name = "MyFoo"]/wsdlsoap:fault[@encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" and @namespace="http://localhost/My/BindingTest" and @use="encoded"]'), 'Found wsdlsoap:fault in wsdl:fault element.');

# one-way operation
ok($xp->exists('/wsdl:definitions/wsdl:binding[@name="MyBindingTestSoapBinding" and @type="impl:MyBindingTestHandler"]/wsdl:operation[@name="testOneway"]/wsdl:input[@name = "testOnewayRequest"]'), 'Found wsdl:input in operation "testOneway" element.');
ok($xp->exists('/wsdl:definitions/wsdl:binding[@name="MyBindingTestSoapBinding" and @type="impl:MyBindingTestHandler"]/wsdl:operation[@name="testOneway"]/wsdl:input[@name = "testOnewayRequest"]/wsdlsoap:body[@encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" and @namespace="http://localhost/My/BindingTest" and @use="encoded"]'), 'Found wsdlsoap:body in wsdl:input element.');

# output
ok(!$xp->exists('/wsdl:definitions/wsdl:binding[@name="MyBindingTestSoapBinding" and @type="impl:MyBindingTestHandler"]/wsdl:operation[@name="testOneway"]/wsdl:output[@name = "testOnewayResponse"]'), 'Did not find wsdl:output in operation "testOneway" element (which is correct).');

# test method without wsdl pod
ok(!$xp->exists('/wsdl:definitions/wsdl:binding[@name="MyBindingTestSoapBinding" and @type="impl:MyBindingTestHandler"]/wsdl:operation[@name="testWithoutPod"]'), 'Non pod operation not found in binding.');

#print $xmlOutput;

__END__
# just to make writing tests easier ...
<wsdl:binding name="MyBindingTestSoapBinding" type="impl:MyBindingTestHandler">
    <wsdlsoap:binding style="rpc" transport="http://schemas.xmlsoap.org/soap/http" />

    <wsdl:operation name="testGeneral">
        <wsdlsoap:operation soapAction="" />
        <wsdl:input name="testGeneralRequest">
                <wsdlsoap:body encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" namespace="http://localhost/My/Test" use="encoded" />
        </wsdl:input>
        <wsdl:output name="testGeneralResponse">
                <wsdlsoap:body encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" namespace="http://localhost/My/Test" use="encoded" />
        </wsdl:output>
        <wsdl:fault name="MyFoo">
            <wsdlsoap:fault encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" namespace="http://localhost/My/Test" use="encoded" />
        </wsdl:fault>
    </wsdl:operation>
</wsdl:binding>

