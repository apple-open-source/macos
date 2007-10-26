#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Lite; 
use HTTP::Cookies;

my $soap = SOAP::Lite
  -> uri('urn:xmethodsInterop')
  -> proxy('http://services.xmethods.net:80/soap/servlet/rpcrouter', 
           cookie_jar => HTTP::Cookies->new(ignore_discard => 1))
;

print $soap->echoString('Hello')->result;
