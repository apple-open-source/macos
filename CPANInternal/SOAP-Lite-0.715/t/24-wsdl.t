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

# ------------------------------------------------------
use SOAP::Test;

$s = SOAP::Lite->uri('http://something/somewhere')->proxy('http://services.xmethods.net/soap/servlet/rpcrouter')->on_fault(sub{});
eval { $s->transport->timeout($SOAP::Test::TIMEOUT = $SOAP::Test::TIMEOUT) };
$r = $s->test_connection;

unless (defined $r && defined $r->envelope) {
  print "1..0 # Skip: ", $s->transport->status, "\n"; 
  exit;
}
# ------------------------------------------------------

plan tests => 12;

{
# Service description (WSDL) (http://www.xmethods.net/)
  print "Service description (WSDL) test(s)...\n";
  $s = SOAP::Lite
    -> service('http://services.xmethods.net/soap/urn:xmethods-delayed-quotes.wsdl');

  ok($s->getQuote('MSFT') > 1);

  ok(SOAP::Lite
    -> service('http://services.xmethods.net/soap/urn:xmethods-delayed-quotes.wsdl')
    -> getQuote('MSFT') > 1);

  # WSDL with <import> element and multiple ports (non-SOAP bindings)
  ok(SOAP::Lite
    -> service('http://www.xmethods.net/sd/StockQuoteImport.wsdl')
    -> getQuote('MSFT') > 1);

  my $schema = SOAP::Schema
    -> schema('http://services.xmethods.net/soap/urn:xmethods-delayed-quotes.wsdl')
    -> parse('net.xmethods.services.stockquote.StockQuoteService');

  foreach (keys %{$schema->services}) {
    eval { $schema->stub($_) } or die;
  }

  # SOAP::Schema converts
  # net.xmethods.services.stockquote.StockQuoteService
  # into
  # net_xmethods_services_stockquote_StockQuoteService

  print "Service description static stub test(s)...\n";
  ok(net_xmethods_services_stockquote_StockQuoteService->getQuote('MSFT') > 1);

  ok(defined net_xmethods_services_stockquote_StockQuoteService->self);

  ok(net_xmethods_services_stockquote_StockQuoteService->self->call);

  print "Service description static stub with import test(s)...\n";
  net_xmethods_services_stockquote_StockQuoteService->import(':all');

  ok(getQuote('MSFT') > 1);

  ok(defined net_xmethods_services_stockquote_StockQuoteService->self);

  ok(net_xmethods_services_stockquote_StockQuoteService->self->call);

  # ok, now we'll test for passing SOAP::Data and SOAP::Headers as a parameters

  my @params;
  {
    package TestStockQuoteService; 
    @TestStockQuoteService::ISA = 'net_xmethods_services_stockquote_StockQuoteService';
    sub call { shift; @params = @_; new SOAP::SOM }
  }

  my @testparams = (SOAP::Data->name(param1 => 'MSFT'), 
                    SOAP::Data->name('param2'),
                    SOAP::Header->name(header1 => 'value'));
  TestStockQuoteService->new->getQuote(@testparams);

  ok($params[1]->value->name eq 'param1');
  ok($params[2]->name eq 'param2');
  ok(ref $params[3] eq 'SOAP::Header' && $params[3]->name eq 'header1');
}
