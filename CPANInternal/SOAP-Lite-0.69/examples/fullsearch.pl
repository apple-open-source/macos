#!perl -w
#!d:\perl\bin\perl.exe 

# -- UDDI::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use UDDI::Lite 
  import => 'UDDI::Data',
  import => 'UDDI::Lite',
  proxy => 'http://www-3.ibm.com/services/uddi/inquiryapi'
;

$\="\n"; $,=", "; # specify separators for print()

my($serviceInfo) = grep { $_->name =~ /stock quote/i } find_business(name('XMethods'))
  -> businessInfos 
  -> businessInfo            
  -> serviceInfos
  -> serviceInfo             
;  

print $serviceInfo->name, $serviceInfo->serviceKey;

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

print $accessPoint, $tModelKey;

my $wsdl = get_tModelDetail($tModelKey)
  -> tModel
  -> overviewDoc
  -> overviewURL
  -> value
;

print $wsdl;

print SOAP::Lite->service($wsdl)->proxy($accessPoint)->getQuote('MSFT');
