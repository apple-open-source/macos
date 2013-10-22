#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

BEGIN { warn "Started...\n" }

# import interface. All methods from loaded service are imported by default
use SOAP::Lite
  service => 'http://services.xmethods.net/soap/urn:xmethods-delayed-quotes.wsdl',
  # service => 'file:/your/local/path/xmethods-delayed-quotes.wsdl',
  # service => 'file:./xmethods-delayed-quotes.wsdl',
;

warn "Loaded...\n";
print getQuote('MSFT'), "\n";
