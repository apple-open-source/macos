#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Lite;

# simple object interface
print SOAP::Lite 
  -> uri('http://simon.fell.com/calc')
  -> proxy('http://soap.4s4c.com/ssss4c/soap.asp')
  -> doubler(SOAP::Data->name(nums => [10,20,30,50,100]))
  -> result ->[1];
