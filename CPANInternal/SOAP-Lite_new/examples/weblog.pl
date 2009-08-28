#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

# example for Weblogs SOAP interface (http://www.soapware.org/weblogsCom)

use SOAP::Lite;

my $weblogs = SOAP::Lite
  -> proxy("http://rpc.weblogs.com/weblogUpdates")
  -> on_action(sub{'"/weblogUpdates"'});

print $weblogs->ping(
  SOAP::Data->name(weblogname=>'Scripting News'),
  SOAP::Data->name(weblogurl=>'http://www.scripting.com/'),
)->result->{message};
