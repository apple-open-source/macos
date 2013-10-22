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
    ref $res ? warn(join " ", "SOAP FAULT:", $res->faultstring, "\n") 
             : warn(join " ", "TRANSPORT ERROR:", $soap->transport->status, "\n");
    return new SOAP::SOM;
  }
;

my($a, $s, $r);

# updated on 2001/08/17
# http://services.xmethods.net:80/soap/servlet/rpcrouter
my $proxy = 'http://nagoya.apache.org:5049/axis/servlet/AxisServlet';

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

plan tests => 15;

{
# XMethod's JavaSOAP server (http://xmethods.net/detail.html?id=11)
  print "XMethod's JavaSOAP server test(s)...\n";
  $s = SOAP::Lite 
    -> uri('http://soapinterop.org/')
    -> proxy($proxy)
  ;

  $a = 'SOAP::Lite';
  $r = $s->echoString($a)->result;
  ok($r eq $a); 

  $a = ['a', 'b'];
  $r = $s->echoStringArray($a)->result;
  ok(ref $r && join('', @$r) eq join('', @$a)); 

  $a = 11;
  $r = $s->echoInteger($a)->result;
  ok($r == $a); 

  $a = [1, 3, 5];
  $r = $s->echoIntegerArray($a)->result;
  ok(ref $r && join('', @$r) == join('', @$a)); 

  $a = 11.02;
  $r = $s->echoFloat($a)->result;
  ok($r == $a); 

  $a = [1.1, 3.3, 5.5];
  $r = $s->echoFloatArray($a)->result;
  ok(ref $r && join('', @$r) eq join('', @$a)); 

  # you may specify URI manually (but see maptype() below)
  $a = {varString => 'b', varInt => 2, varFloat => 95.7};
  $r = $s->echoStruct(
    SOAP::Data->type('xx:SOAPStruct' => $a)
              ->attr({'xmlns:xx' => 'http://soapinterop.org/xsd'})
  )->result;
  ok(ref $r && join('', sort values %$r) eq join('', sort values %$a)); 

  # specify mapping to URI
  $s->maptype({SOAPStruct => 'http://soapinterop.org/xsd'});

  $a = {varString => 'b', varInt => 2, varFloat => 95.7};
  $r = $s->echoStruct($a)->result;
  ok(ref $r && join('', sort values %$r) eq join('', sort values %$a)); 

  $a = {varString => 'b', varInt => 2, varFloat => 95.7};
  $r = $s->echoStruct(SOAP::Data->name(something => $a))->result;
  ok(ref $r && join('', sort values %$r) eq join('', sort values %$a)); 

  $a = [
    {varString => 'b', varInt => 2, varFloat => 95.7}, 
    {varString => 'c', varInt => 3, varFloat => 85.7},
    {varString => 'd', varInt => 4, varFloat => 75.7},
  ];
  $r = $s->echoStructArray($a)->result;
  ok(ref $r && join('', map { sort values %$_ } @$r) eq join('', map { sort values %$_ } @$a)); 

  $a = [
    {varString => 'b', varInt => 2, varFloat => 95.7}, 
    {varString => 'c', varInt => 3, varFloat => 85.7},
    {varString => 'd', varInt => 4, varFloat => 75.7},
  ];
  $r = $s->echoStructArray(SOAP::Data->name(something => $a))->result;
  ok(ref $r && join('', map { sort values %$_ } @$r) eq join('', map { sort values %$_ } @$a)); 

  my $key = "\0\1";
  my $value = 456;

  { local $^W;
    # implicit, warning with -w
    $a = $s->echoMap({a => 123, $key => $value})->result;
    ok($a->{$key} == $value);
  }

  # explicit
  $a = $s->echoMap(SOAP::Data->type(map => {a => 123, $key => $value}))->result;
  ok($a->{$key} == $value);

  { local $^W;
    # implicit, warning with -w
    $a = $s->echoMapArray([{a => 123, $key => $value}, {b => 123, $key => 789}])->result;
    ok($a->[0]->{$key} == $value);
  }

  # explicit
  $a = $s->echoMapArray([SOAP::Data->type(map => {a => 123, $key => $value}), SOAP::Data->type(map => {b => 123, $key => 789})])->result;
  ok($a->[0]->{$key} == $value);
}
