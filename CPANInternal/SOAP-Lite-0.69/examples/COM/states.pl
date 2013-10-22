#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use Win32::OLE;

$soap = Win32::OLE->CreateObject('SOAP.Lite')->new
  or die "SOAP::Lite for COM is not installed";

$soap->proxy('http://localhost/')
     ->uri('http://www.soaplite.com/My/Examples');

print $soap->getStateName(shift || 25)->result;

