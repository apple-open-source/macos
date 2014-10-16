#!/bin/env perl 

BEGIN {
  unless(grep /blib/, @INC) {
    chdir 't' if -d 't';
    unshift @INC, '../lib' if -d '../lib';
  }
}

use strict;
use Test;

use SOAP::Lite
  on_fault => sub {
    my $soap = shift;
    my $res = shift;
    ref $res ? warn(join "\n", "--- SOAP FAULT ---", $res->faultcode, $res->faultstring, '') 
             : warn(join "\n", "--- TRANSPORT ERROR ---", $soap->transport->status, '');
    return new SOAP::SOM;
  }
;

my($a, $s, $r, $serialized, $deserialized);

my $proxy = 'http://localhost:8080/soap/servlet/rpcrouter';

# ------------------------------------------------------
use SOAP::Test;

$s = SOAP::Lite->uri('http://something/somewhere')->proxy($proxy)->on_fault(sub{});
eval { $s->transport->timeout($SOAP::Test::TIMEOUT = $SOAP::Test::TIMEOUT) };
$r = $s->test_connection;

unless (defined $r && defined $r->envelope) {
  print "1..0 # Skip: ", $s->transport->status, "\n"; 
  exit;
}
# ------------------------------------------------------

plan tests => 2;

{
# Local server with Apache SOAP (http://xml.apache.org/soap)
  print "Apache SOAP server test(s)...\n";
  $s = SOAP::Lite 
    -> uri('urn:xmltoday-delayed-quotes')
    -> proxy($proxy)
  ;

  ok($s->getQuote('MSFT')->result > 0);
  ok($s->getQuote(SOAP::Data->name(symbol => 'MSFT'))->result > 0);
}
