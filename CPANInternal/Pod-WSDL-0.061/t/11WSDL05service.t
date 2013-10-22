#!/usr/bin/perl -w
package Pod::WSDL;
use Test::More tests => 4;
BEGIN {use_ok('Pod::WSDL');}
use lib length $0 > 11 ? substr $0, 0, length($0) - 17 : '.';
use strict;
use warnings;
use XML::XPath;

my $p = new Pod::WSDL(source => 'My::ServiceTest',
	               location => 'http://localhost/My/Test',
	               pretty => 1,
	               withDocumentation => 1);

my $xmlOutput = $p->WSDL;
my $xp = XML::XPath->new(xml => $xmlOutput);

#print $xmlOutput;
#print XML::XPath::XMLParser::as_string(($xp->find('/wsdl:definitions/wsdl:service')->get_nodelist())[0])
# test general structure
ok($xp->exists('/wsdl:definitions/wsdl:service[@name="MyServiceTestHandlerService"]'), 'Found wsdl:service element in xml output.');
ok($xp->exists('/wsdl:definitions/wsdl:service[@name="MyServiceTestHandlerService"]/wsdl:port[@binding="impl:MyServiceTestSoapBinding" and @name="MyServiceTest"]'), 'Found wsdl:port in wsdl:service element.');
ok($xp->exists('/wsdl:definitions/wsdl:service[@name="MyServiceTestHandlerService"]/wsdl:port[@binding="impl:MyServiceTestSoapBinding" and @name="MyServiceTest"]/wsdlsoap:address[@location="http://localhost/My/Test"]'), 'Found wsdlsoap:address in wsdl:port element.');

__END__

<wsdl:service name="MyServiceTestHandlerService">
	<wsdl:port binding="impl:MyServiceTestSoapBinding" name="MyServiceTest">
        <wsdlsoap:address location="http://localhost/My/Test" />
	</wsdl:port>
</wsdl:service>
