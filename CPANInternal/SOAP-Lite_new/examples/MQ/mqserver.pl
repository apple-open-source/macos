#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Transport::MQ;

my $server = SOAP::Transport::MQ::Server
  ->new('mq://server:port?Channel=CHAN1;QueueManager=QM_SOAP;RequestQueue=SOAPREQ1;ReplyQueue=SOAPRESP1')
  ->dispatch_to('add')
;

print "Contact to SOAP server\n";
  
do { $server->handle } while sleep 1;

sub add { $_[1] + $_[2] };
