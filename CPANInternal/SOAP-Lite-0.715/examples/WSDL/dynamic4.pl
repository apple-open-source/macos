#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Lite;

# object interface
# WSDL with <import> element and multiple ports (non-SOAP bindings)
print SOAP::Lite
  -> service('http://www.xmethods.net/sd/StockQuoteImport.wsdl')
  -> getQuote('MSFT'), "\n";
