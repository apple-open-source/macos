#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

# ---------------------------------------------------------------------
# Object interaction
# connect to service that described by Graham Glass:
# http://www-106.ibm.com/developerworks/library/ws-peer3/?dwzone=ws
# ---------------------------------------------------------------------

use SOAP::Lite;

my $invoice = bless {name => 'My invoice', amount => 41} => 'Invoice';

my $soap = SOAP::Lite
  -> proxy('http://localhost:8080/soap/servlet/rpcrouter')
  -> maptype({Invoice => 'urn:my_encoding'})
  -> uri('urn:demo2:purchasing');

$a = $soap->receive($invoice)->result;
print 'type: ', ref $a, "\n";
print '  name: ', $a->{name}, "\n";
print '  amount: ', $a->{amount}, "\n";
