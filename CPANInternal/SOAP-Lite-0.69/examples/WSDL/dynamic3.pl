#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Lite;

# object interface
my $service = SOAP::Lite
  -> service('http://services.xmethods.net/soap/urn:xmethods-delayed-quotes.wsdl');

print $service->getQuote('MSFT'), "\n";
