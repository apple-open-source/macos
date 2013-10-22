#!/bin/env perl 

BEGIN {
  unless(grep /blib/, @INC) {
    chdir 't' if -d 't';
    unshift @INC, '../lib' if -d '../lib';
  }
}

use strict;
use Test;

BEGIN { plan tests => 8 }

use XMLRPC::Lite;

my($a, $s, $r, $serialized, $deserialized);

{
  print "XML-RPC deserialization test(s)...\n";

  $deserialized = XMLRPC::Deserializer->deserialize('<?xml version="1.0" encoding="ISO-8859-1"?>
<methodCall><methodName>SOAP.Lite.Bug.Report</methodName><params>
<param><value><struct><member><name>This</name><value>Works</value></member></struct></value></param>
</params></methodCall>
  ')->root;

  ok($deserialized->{params}->[0]->{This} eq 'Works');

  $serialized = XMLRPC::Serializer->serialize({param1 => 'value1', param2 => undef, param3 => 0});

  ok($serialized =~ m!<member><name>param2</name><value /></member>!);
  ok($serialized =~ m!<member><name>param3</name><value><int>0</int></value></member>!);

  $deserialized = XMLRPC::Deserializer->deserialize($serialized)->root;

  ok($deserialized->{param2} eq '');
  ok($deserialized->{param3} == 0);

  $serialized = XMLRPC::Serializer->method(a => {param1 => 'value1', param2 => undef, param3 => 'value3'});

  ok($serialized =~ m!<methodCall><methodName>a</methodName><params><param><value><struct>(<member><name>param1</name><value><string>value1</string></value></member>|<member><name>param2</name><value /></member>|<member><name>param3</name><value><string>value3</string></value></member>){3}</struct></value></param></params></methodCall>!);

  $serialized = XMLRPC::Serializer->method(a => {param1 => 'value1'});

  ok($serialized eq '<?xml version="1.0" encoding="UTF-8"?><methodCall><methodName>a</methodName><params><param><value><struct><member><name>param1</name><value><string>value1</string></value></member></struct></value></param></params></methodCall>');

  eval { XMLRPC::Serializer->serialize(XMLRPC::Data->type(base63 => 1)) };

  ok($@ =~ /unsupported datatype/);
}
