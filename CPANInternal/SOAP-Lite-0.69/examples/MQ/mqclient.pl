#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Lite;

$s = SOAP::Lite
  ->proxy('mq://server:port?Channel=CHAN1;QueueManager=QM_SOAP;RequestQueue=SOAPREQ1;ReplyQueue=SOAPRESP1')
;

print $s->add(1,2)->result;

