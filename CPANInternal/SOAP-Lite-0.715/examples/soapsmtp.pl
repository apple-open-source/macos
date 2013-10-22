#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Lite;

SOAP::Lite
  -> uri('http://www.soaplite.com/My/Examples')                
  -> proxy('mailto:destination.email@address', smtp => 'smtp.server', From => 'your.email', Subject => 'SOAP message')

  # or 
  # -> proxy('mailto:destination.email@address?From=your.email&Subject=SOAP%20message', smtp => 'smtp.server')

  # or if you want to send with sendmail
  # -> proxy('mailto:destination.email@address?From=your.email&Subject=SOAP%20message')

  # or if your sendmail is in undiscoverable place
  # -> proxy('mailto:destination.email@address?From=your.email&Subject=SOAP%20message', sendmail => 'command to run your sendmail')

  -> getStateName(12);
