#!/bin/env perl 

BEGIN {
  unless(grep /blib/, @INC) {
    chdir 't' if -d 't';
    unshift @INC, '../lib' if -d '../lib';
  }
}

use strict;
use Test;

BEGIN { plan tests => 47 }

use SOAP::Lite;

my($a, $s, $r, $serialized, $deserialized);

{ # check deserialization of envelope with result
  print "Deserialization of envelope with result test(s)...\n";

  $deserialized = SOAP::Deserializer->deserialize('<?xml version="1.0"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
	 xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
	 xmlns:xsd="http://www.w3.org/2001/XMLSchema"
	 xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
	SOAP-ENV:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
<SOAP-ENV:Body>
<m:doublerResponse xmlns:m="http://simon.fell.com/calc">
<nums xsi:type="SOAP-ENC:Array" SOAP-ENC:arrayType="xsd:int[5]">
<item xsi:type="xsd:int">20</item>
<item xsi:type="xsd:int">40</item>
<item xsi:type="xsd:int">60</item>
<item xsi:type="xsd:int">100</item>
<item xsi:type="xsd:int">200</item>
</nums>
</m:doublerResponse>
</SOAP-ENV:Body>
</SOAP-ENV:Envelope>
');

  ok($deserialized->result->[2] == 60);
  ok((my @array = $deserialized->paramsall) == 1);
  ok(ref $deserialized->body eq 'HASH'); # not blessed anymore since 0.51
}

{ 
  print "hex encoding test(s)...\n";

  $a = "\0 {a}\1";
  $serialized = SOAP::Serializer->serialize(SOAP::Data->type(hex => $a));

  ok($serialized =~ />00207B617D01</);
  ok(SOAP::Deserializer->deserialize($serialized)->root eq $a);
}

{
  print "Deserialization of 1999/2001 schemas test(s)...\n";

  foreach (split "\n", <<EOX) {
<i xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:type="SOAP-ENC:integer">12</i>
<i xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:type="xsd:nonNegativeInteger">12</i>
<i xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:type="xsd:anySimpleType">12</i>
<i xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:type="xsd:anyType">12</i>
<i xmlns:xsd="http://www.w3.org/1999/XMLSchema" xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance" xsi:type="xsd:ur-type">12</i>
<SOAP-ENC:integer xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/">12</SOAP-ENC:integer>
<i>12</i>
EOX
    $deserialized = SOAP::Deserializer->deserialize($_);
    ok($deserialized->root == 12);
  }

  eval { SOAP::Deserializer->deserialize('<i xmlns:xsd="http://www.w3.org/1999/XMLSchema" xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance" xsi:type="xsd:something">12</i>') };
  ok($@ =~ m!Unrecognized type '\{http://www.w3.org/1999/XMLSchema\}something'!);

  eval { SOAP::Deserializer->deserialize('<i xmlns:xsd="http://some.thing.else/XMLSchema" xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance" xsi:type="xsd:something">12</i>') };
  ok($@ =~ m!Unrecognized type '{http://some.thing.else/XMLSchema}something'!);

  foreach (qw(base64Binary hexBinary anyType anySimpleType
      gMonth gYearMonth gYear gMonthDay gDay duration anyURI dateTime)) {
    eval { SOAP::Deserializer->deserialize(qq!<i xmlns:xsd="http://www.w3.org/1999/XMLSchema" xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance" xsi:type="xsd:$_">12</i>!) };
    ok($@ =~ m!Unrecognized type '{http://www.w3.org/1999/XMLSchema}$_'!);
  }

  ok(SOAP::Deserializer->deserialize(qq!<i xmlns:xsd="http://www.w3.org/1999/XMLSchema" xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance" xsi:type="xsd:boolean">true</i>!)->root eq '1');

  eval { SOAP::Deserializer->deserialize(qq!<i xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:type="xsd:boolean">something</i>!) };
  ok($@ =~ m!Wrong boolean value!);

  ok(SOAP::Deserializer->deserialize(qq!<i xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:type="xsd:boolean">true</i>!)->root eq '1');
  ok(SOAP::Deserializer->deserialize(qq!<i xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:type="xsd:boolean">1</i>!)->root eq '1');
  ok(SOAP::Deserializer->deserialize(qq!<i xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:type="xsd:boolean">false</i>!)->root eq '0');
  ok(SOAP::Deserializer->deserialize(qq!<i xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:type="xsd:boolean">0</i>!)->root eq '0');

  foreach (qw(ur-type base64 hex
    timeDuration uriReference timeInstant month year recurringDate recurringDay)) {
    eval { SOAP::Deserializer->deserialize(qq!<i xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:type="xsd:$_">12</i>!) };
    ok($@ =~ m!Unrecognized type '{http://www.w3.org/2001/XMLSchema}$_'!);
  }

  eval { SOAP::Deserializer->deserialize(qq!<i xmlns:xsd="http://www.w3.org/1999/XMLSchema" xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance" xsi:type="xsd:boolean">something</i>!) };
  ok($@ =~ m!Wrong boolean value!);

  ok(SOAP::Deserializer->deserialize(qq!<i xmlns:xsd="http://www.w3.org/1999/XMLSchema" xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance" xsi:type="xsd:boolean">true</i>!)->root eq '1');
  ok(SOAP::Deserializer->deserialize(qq!<i xmlns:xsd="http://www.w3.org/1999/XMLSchema" xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance" xsi:type="xsd:boolean">1</i>!)->root eq '1');
  ok(SOAP::Deserializer->deserialize(qq!<i xmlns:xsd="http://www.w3.org/1999/XMLSchema" xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance" xsi:type="xsd:boolean">false</i>!)->root eq '0');
  ok(SOAP::Deserializer->deserialize(qq!<i xmlns:xsd="http://www.w3.org/1999/XMLSchema" xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance" xsi:type="xsd:boolean">0</i>!)->root eq '0');
}
