use strict;
use warnings;

package DoThingsWithStuff;

use SOAP::Lite;
our @ISA = qw(SOAP::Server::Parameters);

sub new {
    my ( $class ) = @_;
    my $self = bless {}, ref($class) || $class;
    $self;
}

sub do_something {
    my $self = shift;
    my $som = pop;
    #$som->context->serializer->encodingStyle(""); # does adding this affect the fix?
    return SOAP::Data->name(wotsit => "doofer");
}

package main;

use Test::More;
use SOAP::Lite;

eval {
	require Test::XML;
};
if ($@) {
	plan skip_all => 'Cannot test without Test::XML ' . $@;
	exit 0;
}


my $req11 = '<soapenv:Envelope xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/" xmlns:urn="urn:Something"><soapenv:Header/><soapenv:Body><urn:do_something><stuff>things</stuff></urn:do_something></soapenv:Body></soapenv:Envelope>';

my $req12 = '<soapenv:Envelope xmlns:soapenv="http://www.w3.org/2003/05/soap-envelope" xmlns:urn="urn:Something"><soapenv:Header/><soapenv:Body><urn:do_something><stuff>things</stuff></urn:do_something></soapenv:Body></soapenv:Envelope>';

my $expected_11 = '<?xml version="1.0" encoding="UTF-8"?><soap:Envelope xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/" xmlns:xsd="http://www.w3.org/2001/XMLSchema" soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"><soap:Body><do_somethingResponse xmlns="urn:Something"><wotsit xsi:type="xsd:string">doofer</wotsit></do_somethingResponse></soap:Body></soap:Envelope>';

my $expected_12 = '<?xml version="1.0" encoding="UTF-8"?><soap:Envelope xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:soapenc="http://www.w3.org/2003/05/soap-encoding" xmlns:xsd="http://www.w3.org/2001/XMLSchema" soap:encodingStyle="http://www.w3.org/2003/05/soap-encoding" xmlns:soap="http://www.w3.org/2003/05/soap-envelope"><soap:Body><do_somethingResponse xmlns="urn:Something"><wotsit xsi:type="xsd:string">doofer</wotsit></do_somethingResponse></soap:Body></soap:Envelope>';

my $t = DoThingsWithStuff->new;

my $s = SOAP::Server->new;
$s->dispatch_with({
    "urn:Something" => $t
});

my $res_11_1 = $s->handle($req11);
Test::XML::is_xml($res_11_1, $expected_11, "Got correct SOAP 1.1 response");

my $res_12 = $s->handle($req12);
Test::XML::is_xml($res_12, $expected_12, "Got correct SOAP 1.2 response");

my $res_11_2 = $s->handle($req11);
Test::XML::is_xml($res_11_2, $expected_11, "Got correct SOAP 1.1 response after processing a SOAP 1.2 request");

done_testing;
