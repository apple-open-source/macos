#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Lite;

$r = SOAP::Lite 
  -> uri('urn:lemurlabs-Fortune')
  -> proxy('http://www.lemurlabs.com/rpcrouter')
  -> getFortuneByDictionary('work')
  -> result || '';

print $r && ref($r = SOAP::Deserializer->deserialize($r)) && ($r = $r->valueof('//fortune') || '') 
  ? "Your fortune cookie:\n$r" : "No fortune cookies for you today\n";
