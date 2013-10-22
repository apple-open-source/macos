#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Lite;

# using WSDL
print SOAP::Lite
  -> service('http://soap.4s4c.com/ssss4c/soap.asp?WSDL')
  -> doubler([10,20,30,50,100])->[2], "\n";
