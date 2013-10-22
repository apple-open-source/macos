#!perl -w
#!d:\perl\bin\perl.exe 

# -- UDDI::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use UDDI::Lite 
  import => 'UDDI::Data', 
  import => 'UDDI::Lite',
  proxy => 'http://uddi.microsoft.com/inquire'
;

print find_business(name('xmethods'))
  -> businessInfos->businessInfo->serviceInfos->serviceInfo->name;                         
