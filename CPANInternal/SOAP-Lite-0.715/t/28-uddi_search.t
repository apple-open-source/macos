#!/bin/env perl 

BEGIN {
  unless(grep /blib/, @INC) {
    chdir 't' if -d 't';
    unshift @INC, '../lib' if -d '../lib';
  }
}

use strict;
use Test;

use UDDI::Lite 
  import => 'UDDI::Data',
  import => 'UDDI::Lite',
  proxy => 'http://www-3.ibm.com/services/uddi/inquiryapi'
;

my($a, $s, $r, $serialized, $deserialized);

# ------------------------------------------------------
use SOAP::Test;

$s = SOAP::Lite->uri('http://something/somewhere')->proxy('http://www-3.ibm.com/services/uddi/inquiryapi')->on_fault(sub{});
eval { $s->transport->timeout($SOAP::Test::TIMEOUT = $SOAP::Test::TIMEOUT) };
$r = $s->test_connection;

unless (defined $r && defined $r->envelope) {
  print "1..0 # Skip: ", $s->transport->status, "\n"; 
  exit;
}
# ------------------------------------------------------

my($serviceInfo) = grep { $_->name =~ /stock quote/i } find_business(name('XMethods'))
  -> businessInfos 
  -> businessInfo            
  -> serviceInfos
  -> serviceInfo             
;  

unless (defined $serviceInfo) {
  print "1..0 # Skip: getQuote service doesn't exist on XMethods\n";
  exit;
}

plan tests => 6;

ok($serviceInfo->name);
ok($serviceInfo->serviceKey);
print $serviceInfo->name, ", ", $serviceInfo->serviceKey, "\n";

my $bindingTemplate = get_serviceDetail($serviceInfo->serviceKey)
  -> businessService
  -> bindingTemplates
  -> bindingTemplate         
;

my $accessPoint = $bindingTemplate->accessPoint->value;
my $tModelKey = $bindingTemplate
  -> tModelInstanceDetails
  -> tModelInstanceInfo
  -> tModelKey
;

ok($accessPoint);
ok($tModelKey);
print $accessPoint, ", ", $tModelKey, "\n";

my $wsdl = get_tModelDetail($tModelKey)
  -> tModel
  -> overviewDoc
  -> overviewURL
  -> value
;

ok($wsdl);
print $wsdl, "\n";

my $quote = SOAP::Lite->service($wsdl)->proxy($accessPoint)->getQuote('MSFT');
ok($quote > 0);
print $quote, "\n";
